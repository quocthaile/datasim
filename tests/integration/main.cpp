#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace datasim;
namespace fs = std::filesystem;
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
        std::cout << "PASS: samples=" << samples.size() << " max_error_V=" << report.max_absolute_error_V
            << " rmse_V=" << report.rmse_V << " artifacts=" << dir.string() << '\n';
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
