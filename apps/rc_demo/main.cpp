#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
namespace fs = std::filesystem;
using namespace datasim;
int main(int argc, char** argv) {
    fs::path directory;
    std::ofstream metadata;
    std::string simulation_status = "NOT_RUN", validation_status = "NOT_RUN";
    try {
        RcConfig c; std::string executable = "ngspice", output;
        for (int i = 1; i < argc; ++i) {
            const std::string option = argv[i];
            if (option == "--help") {
                std::cout << "rc_demo --output NEW_DIRECTORY [--ngspice EXECUTABLE] [--r-ohm R] [--c-f C] [--voltage-v V] [--step-s T] [--duration-s T]\n";
                return 0;
            }
            if (++i >= argc) throw std::invalid_argument("missing option value");
            const std::string value = argv[i];
            if (option == "--output") output = value;
            else if (option == "--ngspice") executable = value;
            else {
                std::istringstream in(value); in.imbue(std::locale::classic()); double number; std::string extra;
                if (!(in >> number) || (in >> extra)) throw std::invalid_argument("invalid numeric option: " + option);
                if (option == "--r-ohm") c.resistance_ohm = number;
                else if (option == "--c-f") c.capacitance_F = number;
                else if (option == "--voltage-v") c.voltage_V = number;
                else if (option == "--step-s") c.step_s = number;
                else if (option == "--duration-s") c.duration_s = number;
                else throw std::invalid_argument("unsupported option: " + option);
            }
        }
        check_config(c);
        if (output.empty()) throw std::invalid_argument("--output NEW_DIRECTORY is required");
        directory = fs::absolute(output);
        fs::create_directories(directory.parent_path());
        if (!fs::create_directory(directory)) throw std::runtime_error("output directory already exists; use a new directory");
        metadata.open(directory / "metadata.txt"); metadata.exceptions(std::ios::badbit | std::ios::failbit);
        metadata.imbue(std::locale::classic()); metadata << std::setprecision(17);
        metadata << "backend=ngspice\nparameter_origin=manually_configured_ideal_components\n"
            << "resistance_ohm=" << c.resistance_ohm << "\ncapacitance_F=" << c.capacitance_F
            << "\nsource_initial_V=0\nsource_final_V=" << c.voltage_V << "\ncapacitor_initial_V=" << c.initial_voltage_V
            << "\ndelay_s=" << c.delay_s << "\nrise_s=" << c.rise_s << "\nfall_s=" << c.fall_s
            << "\nwidth_s=" << c.width_s << "\nperiod_s=" << c.period_s << "\nstep_s=" << c.step_s
            << "\nmaximum_step_s=" << c.step_s << "\nduration_s=" << c.duration_s
            << "\nreltol=1e-6\nabstol_A=1e-12\nvntol_V=1e-9\nmethod=trap\ntemperature_degC=27\nnominal_temperature_degC=27\n"
            << "validation_start_s=" << c.delay_s + c.rise_s << "\nvalidation_end_s=" << c.duration_s
            << "\ntolerance_V=" << c.voltage_V * 0.01 << "\noutput_file=results.csv\n";
        const auto exe = find_executable(executable);
        if (exe.empty()) throw std::runtime_error("ngspice executable not found: " + executable);
        metadata << "backend_executable=" << exe.string() << "\nbackend_version_begin\n"
            << backend_version(exe, directory) << "backend_version_end\n";
        simulation_status = "FAILED";
        const auto samples = run_ngspice(c, exe, directory);
        simulation_status = "SUCCEEDED";
        { std::ofstream csv(directory / "results.csv"); write_csv(csv, samples); csv.close();
          if (!csv) throw std::runtime_error("CSV close failed"); }
        validation_status = "FAILED";
        const auto report = validate(c, samples);
        { std::ofstream csv(directory / "errors.csv"); write_errors(csv, report); csv.close();
          if (!csv) throw std::runtime_error("error CSV close failed"); }
        metadata << "sample_count=" << samples.size() << "\nvalidation_sample_count=" << report.errors.size()
            << "\nmax_absolute_error_V=" << report.max_absolute_error_V << "\nrmse_V=" << report.rmse_V
            << "\nnormalized_rmse=" << report.normalized_rmse << '\n';
        if (!report.passed) throw std::runtime_error("numerical validation failed: maximum absolute error exceeds tolerance");
        validation_status = "PASSED";
        metadata << "simulation_status=" << simulation_status << "\nvalidation_status=" << validation_status << '\n';
        metadata.close();
        std::cout << std::setprecision(12) << "samples=" << samples.size() << " validation_samples=" << report.errors.size()
            << "\nmax_absolute_error_V=" << report.max_absolute_error_V << "\nrmse_V=" << report.rmse_V
            << "\nnormalized_rmse=" << report.normalized_rmse << "\ntolerance_V=" << report.tolerance_V
            << "\nvalidation=PASSED\noutput=" << directory.string() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        if (metadata.is_open()) {
            try { metadata << "simulation_status=" << simulation_status << "\nvalidation_status=" << validation_status
                           << "\nerror=" << e.what() << '\n'; metadata.close(); } catch (...) {}
        }
        return 1;
    }
}
