#include "read_only_samples.hpp"
#include "workbench_service.hpp"
#include "datasim/simulation/io/report.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string read_text(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("expected ngspice executable and isolated test directory");
        datasim::RcConfig defaults;
        datasim::check_config(defaults);
        require(datasim::time_constant(defaults) == 1e-3, "default RC time constant changed");

        const std::array<std::string, 11> custom{
            "10000", "1e-6", "5", "0", "0.001", "1e-9", "1e-9",
            "0.01", "0.02", "1e-5", "0.006"};
        const auto config = datasim_viewer::parse_rc_config(custom);
        require(config.resistance_ohm == 10000 && config.rise_s == 1e-9,
                "valid custom RC configuration lost value precision");
        for (const std::string malformed : {"", "1,5", "1.0x", " nan", "NaN", "inf"}) {
            bool rejected = false;
            try { (void)datasim_viewer::parse_decimal(malformed); }
            catch (const std::invalid_argument&) { rejected = true; }
            require(rejected, "strict numeric parser accepted malformed, locale-ambiguous, or non-finite input");
        }
        auto invalid = custom;
        invalid[0] = "0";
        bool invalid_rejected = false;
        try { (void)datasim_viewer::parse_rc_config(invalid); }
        catch (const std::invalid_argument&) { invalid_rejected = true; }
        require(invalid_rejected, "datasim::check_config did not reject invalid resistance");
        invalid = custom;
        invalid[9] = "0.002";
        invalid_rejected = false;
        try { (void)datasim_viewer::parse_rc_config(invalid); }
        catch (const std::invalid_argument&) { invalid_rejected = true; }
        require(invalid_rejected, "datasim::check_config did not reject unsupported timestep");

        const fs::path parent = argv[2];
        fs::create_directories(parent);
        bool parent_rejected = false;
        try { (void)datasim_viewer::fresh_run_directory(parent / "absent-parent"); }
        catch (const std::invalid_argument&) { parent_rejected = true; }
        require(parent_rejected && !fs::exists(parent / "absent-parent"),
                "invalid output parent was created or accepted");

        if (std::string(argv[1]).empty() || !fs::exists(argv[1])) return 77;
        const auto first_dir = datasim_viewer::fresh_run_directory(parent);
        const auto second_dir = datasim_viewer::fresh_run_directory(parent);
        require(first_dir != second_dir && !fs::exists(first_dir) && !fs::exists(second_dir),
                "run path policy did not produce distinct not-yet-created paths");
        const auto first = datasim::execute_rc_run(config, argv[1], first_dir);
        const auto second = datasim::execute_rc_run(config, argv[1], second_dir);
        require(first.validation.passed && second.validation.passed, "real backend run did not pass validation");
        require(!first.samples.empty() && std::isfinite(first.backend_seconds), "run result samples/time invalid");
        require(first.samples.size() == datasim_viewer::load_samples_csv(first_dir / "results.csv").samples.size(),
                "successful RunResult did not match the read-only waveform CSV");
        for (const char* artifact : {"results.csv", "errors.csv", "circuit.cir", "run_metadata.json", "validation_report.json"}) {
            require(fs::is_regular_file(first_dir / artifact), "successful run artifact is missing");
            require(fs::is_regular_file(second_dir / artifact), "second run artifact is missing");
        }
        const auto validation_text = read_text(first_dir / "validation_report.json");
        require(validation_text.find("\"maximum_absolute_error_V\"") != std::string::npos &&
                validation_text.find("\"rmse_V\"") != std::string::npos &&
                validation_text.find("\"tolerance_V\"") != std::string::npos,
                "run validation artifact is missing returned metric fields");

        const auto protected_dir = parent / "already-exists";
        fs::create_directory(protected_dir);
        { std::ofstream sentinel(protected_dir / "user.txt"); sentinel << "preserve"; }
        bool collision_rejected = false;
        try { (void)datasim::execute_rc_run(config, argv[1], protected_dir); }
        catch (const std::exception&) { collision_rejected = true; }
        require(collision_rejected && read_text(protected_dir / "user.txt") == "preserve",
                "existing output directory was overwritten");

        std::cout << "PASS: workbench parsing, core validation, real ngspice runs, fresh output paths, metrics, artifacts, collision safety\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "viewer workbench test failure: " << error.what() << '\n';
        return 1;
    }
}
