#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include "datasim/simulation/io/report.hpp"
#include "datasim/vertical/provenance/run_provenance.hpp"
#include "../unit/vertical_slice_fixtures.hpp"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace datasim;
namespace fs = std::filesystem;
static std::string read_file_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) throw std::runtime_error("missing integration artifact: " + p.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
    try {
        const bool explicit_path = argc > 1 && std::string(argv[1]).size();
        const auto exe = find_executable(explicit_path ? argv[1] : "ngspice");
        if (exe.empty()) {
            if (explicit_path) throw std::runtime_error("configured ngspice executable not found");
            std::cout << "SKIP: ngspice not found on PATH; set DATASIM_NGSPICE to an executable\n"; return 77;
        }
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto dir = fs::absolute("integration run " + std::to_string(stamp));
        if (!fs::create_directory(dir)) throw std::runtime_error("integration directory collision");
        std::cout << backend_version(exe, dir);
        const auto samples = run_ngspice(RcConfig{}, exe, dir);
        { std::ofstream out(dir / "results.csv"); write_csv(out, samples); }
        std::ifstream in(dir / "results.csv"); const auto parsed = read_csv(in);
        const auto report = validate(RcConfig{}, parsed);
        if (!report.passed) throw std::runtime_error("RC tolerance failed");
        if (parsed.size() != samples.size()) throw std::runtime_error("CSV roundtrip count mismatch");
        // A second run may not consume stale output.
        bool rejected = false;
        try { run_ngspice(RcConfig{}, exe, dir); } catch (const std::exception&) { rejected = true; }
        if (!rejected) throw std::runtime_error("stale artifact guard failed");
        const auto run=execute_rc_run(RcConfig{},exe.string(),dir/"reported-run");
        if (!run.validation.passed || run.validation.errors.size()!=4001 || !fs::exists(dir/"reported-run"/"validation_report.json"))
            throw std::runtime_error("reported common-grid run failed");
        // Phase 3 knowledge-driven vertical slice execution and equivalence check against manual RcConfig.
        const auto assembly = vertical::testing::make_validated_synthetic_rc_assembly();
        const auto v_inputs = vertical::testing::make_synthetic_test_run_inputs();
        const auto v_run = vertical::execute_vertical_rc_run(assembly, v_inputs, exe.string(), dir / "vertical-run");
        if (!v_run.run.validation.passed || v_run.run.validation.errors.size() != 4001 ||
            !fs::exists(dir / "vertical-run" / "provenance_metadata.json") ||
            !fs::exists(dir / "vertical-run" / "validation_report.json"))
            throw std::runtime_error("vertical slice common-grid run failed");
        if (v_run.run.samples.size() != run.samples.size())
            throw std::runtime_error("vertical slice sample count differs from manual RC run");
        for (std::size_t i = 0; i < run.samples.size(); ++i) {
            if (v_run.run.samples[i].time_s != run.samples[i].time_s ||
                v_run.run.samples[i].vin_V != run.samples[i].vin_V ||
                v_run.run.samples[i].vout_V != run.samples[i].vout_V)
                throw std::runtime_error("vertical slice waveform differs from equivalent manual RC run");
        }
        if (read_file_bytes(dir / "vertical-run" / "results.csv") != read_file_bytes(dir / "reported-run" / "results.csv") ||
            read_file_bytes(dir / "vertical-run" / "errors.csv") != read_file_bytes(dir / "reported-run" / "errors.csv") ||
            read_file_bytes(dir / "vertical-run" / "circuit.cir") != read_file_bytes(dir / "reported-run" / "circuit.cir"))
            throw std::runtime_error("vertical slice generated artifacts differ from equivalent manual RC run");
        const auto prov_text = read_file_bytes(dir / "vertical-run" / "provenance_metadata.json");
        if (prov_text.find("\"execution_status\":\"SUCCEEDED\"") == std::string::npos ||
            prov_text.find("\"validation_status\":\"PASSED\"") == std::string::npos ||
            prov_text.find("\"execution_classification\":\"test_only_synthetic\"") == std::string::npos)
            throw std::runtime_error("vertical slice provenance metadata incomplete");
        const auto bad=unique_workspace(dir);
        { std::ofstream out(bad/"invalid.cir"); out << "Invalid netlist regression\nR1 vin vout not_a_value\n.tran 1u 1m\n.end\n"; }
        const int exit=run_process(exe,{"-n","-b","invalid.cir"},bad,bad/"invalid.log");
        if (exit==0) throw std::runtime_error("ngspice accepted invalid netlist");
        std::cout << "PASS: real invalid netlist rejected with exit " << exit << '\n';
        std::cout << "PASS: vertical slice identical to manual RC (samples=" << v_run.run.samples.size()
            << " max_error_V=" << v_run.run.validation.max_absolute_error_V
            << " rmse_V=" << v_run.run.validation.rmse_V << ")\n";
        std::cout << "PASS: samples=" << samples.size() << " max_error_V=" << report.max_absolute_error_V
            << " rmse_V=" << report.rmse_V << " artifacts=" << dir.string() << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
