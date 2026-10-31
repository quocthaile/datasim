#include "report.hpp"
#include "datasim/build_info.hpp"
#include "datasim/simulation/analysis/grid.hpp"
#include "datasim/simulation/analysis/reference.hpp"
#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace datasim {
namespace fs = std::filesystem;
static std::ostringstream stream() { std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17); return out; }
std::string json_string(const std::string& text) {
    auto out = stream(); out << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned>(c) << std::dec;
        else out << c;
    }
    out << '"'; return out.str();
}
std::string config_json(const RcConfig& c) {
    check_config(c); auto out = stream();
    out << "{\"parameter_origin\":\"manual_ideal_components\",\"resistance_ohm\":" << c.resistance_ohm
        << ",\"capacitance_F\":" << c.capacitance_F << ",\"voltage_V\":" << c.voltage_V
        << ",\"initial_voltage_V\":" << c.initial_voltage_V << ",\"source_initial_V\":0,\"delay_s\":" << c.delay_s
        << ",\"rise_s\":" << c.rise_s << ",\"fall_s\":" << c.fall_s << ",\"width_s\":" << c.width_s
        << ",\"period_s\":" << c.period_s << ",\"requested_step_s\":" << c.step_s
        << ",\"maximum_step_s\":" << c.step_s << ",\"duration_s\":" << c.duration_s
        << ",\"method\":\"trap\",\"reltol\":1e-6,\"abstol_A\":1e-12,\"vntol_V\":1e-9,\"temperature_degC\":27,\"tnom_degC\":27}";
    return out.str();
}
std::string content_id(const std::string& text) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ULL; }
    auto out = stream(); out << std::hex << std::setw(16) << std::setfill('0') << hash; return out.str();
}
fs::path unique_workspace(const fs::path& parent) {
    fs::create_directories(parent);
    const auto prefix = "run-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    for (int attempt=0; attempt<1000; ++attempt) {
        auto path = fs::absolute(parent/(prefix+"-"+std::to_string(attempt)));
        if (fs::create_directory(path)) return path;
    }
    throw std::runtime_error("cannot reserve unique run workspace");
}
static void save(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary); out.exceptions(std::ios::failbit|std::ios::badbit);
    out << text; out.close();
}
static std::string metrics(const Validation& v) {
    auto out=stream();
    out << "{\"maximum_absolute_error_V\":" << v.max_absolute_error_V << ",\"rmse_V\":" << v.rmse_V
        << ",\"nrmse\":" << v.normalized_rmse << ",\"sample_count\":" << v.errors.size()
        << ",\"tolerance_V\":" << v.tolerance_V << ",\"passed\":" << (v.passed?"true":"false") << '}';
    return out.str();
}
RunResult execute_rc_run(const RcConfig& c, const std::string& executable, const fs::path& new_directory) {
    check_config(c); // Before any workspace, files or process activity.
    const auto dir=fs::absolute(new_directory); fs::create_directories(dir.parent_path());
    if (!fs::create_directory(dir)) throw std::runtime_error("output directory already exists; use a new directory");
    std::string execution="NOT_RUN", validation="NOT_RUN", version, error, exe;
    double seconds=0;
    const auto metadata = [&] {
        auto out=stream(); out << "{\n\"schema_version\":1,\"backend\":\"ngspice\",\"backend_version\":" << json_string(version)
            << ",\n\"backend_executable\":" << json_string(exe) << ",\"configuration\":" << config_json(c)
            << ",\n\"netlist_hash_algorithm\":\"FNV-1a-64 (non-cryptographic)\",\"netlist_hash\":" << json_string(content_id(rc_netlist(c)))
            << ",\"application_version\":" << json_string(DATASIM_VERSION) << ",\"git_revision_at_configure\":" << json_string(DATASIM_GIT_REVISION)
            << ",\"source_sha256\":" << json_string(DATASIM_SOURCE_SHA256) << ",\"compiler\":" << json_string(DATASIM_COMPILER)
            << ",\"build_configuration\":" << json_string(DATASIM_BUILD_TYPE) << ",\"platform\":" << json_string(DATASIM_PLATFORM)
            << ",\n\"backend_seconds\":" << seconds << ",\"execution_status\":" << json_string(execution)
            << ",\"validation_status\":" << json_string(validation) << ",\"error\":" << json_string(error)
            << ",\"output_file\":\"results.csv\",\"retention\":\"retain all artifacts; no automatic file deletion\"\n}\n";
        save(dir/"run_metadata.json",out.str());
    };
    try {
        metadata();
        const auto found=find_executable(executable); exe=found.string();
        if (found.empty()) throw std::runtime_error("ngspice executable not found: "+executable);
        version=backend_version(found,dir);
        execution="FAILED"; const auto start=std::chrono::steady_clock::now();
        auto samples=run_ngspice(c,found,dir);
        seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count(); execution="SUCCEEDED";
        auto csv=stream(); write_csv(csv,samples); save(dir/"results.csv",csv.str());
        validation="FAILED";
        auto result=validate_on_grid(c,samples,validation_grid(c));
        auto errors=stream(); write_errors(errors,result); save(dir/"errors.csv",errors.str());
        const auto native=validate(c,samples);
        auto report=stream();
        report << "{\"schema_version\":1,\"status\":" << json_string(result.passed?"PASSED":"FAILED")
            << ",\"interval_start_s\":" << c.delay_s+c.rise_s << ",\"interval_end_s\":" << c.duration_s
            << ",\"grid\":\"4000 equal intervals, 4001 points including endpoints\",\"interpolation\":\"piecewise linear; no extrapolation\""
            << ",\"common_grid\":" << metrics(result) << ",\"native_grid\":" << metrics(native) << ",\"checkpoints\":[";
        bool first=true;
        for (double multiple : {0.,1.,2.,3.,5.}) {
            const double t=c.delay_s+multiple*time_constant(c);
            if (t>c.duration_s) continue;
            const double measured=interpolate(samples,{t}).front().vout_V;
            const double ideal=analytical_voltage(c,t);
            if (!first) report << ',';
            first=false;
            report << "{\"tau_multiple\":" << multiple << ",\"time_s\":" << t << ",\"simulation_V\":" << measured
                << ",\"ideal_V\":" << ideal << ",\"ramp_reference_V\":" << ramp_voltage(c,t)
                << ",\"error_V\":" << measured-ideal << '}';
        }
        report << "]}\n"; save(dir/"validation_report.json",report.str());
        if (!result.passed) throw std::runtime_error("numerical validation failed; tolerance unchanged at 1% amplitude");
        validation="PASSED"; metadata(); return {std::move(samples),std::move(result),seconds};
    } catch (const std::exception& e) {
        error=e.what();
        // Preserve a detailed computed failure report when one already exists.
        if (!fs::exists(dir/"validation_report.json")) save(dir/"validation_report.json",
            "{\"schema_version\":1,\"status\":"+json_string(validation)+",\"error\":"+json_string(error)+"}\n");
        metadata(); throw;
    }
}
}
