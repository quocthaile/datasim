#include "datasim/simulation/io/report.hpp"
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
        const auto run = execute_rc_run(c, executable, directory);
        const auto& samples = run.samples;
        const auto& report = run.validation;
        std::cout << std::setprecision(12) << "samples=" << samples.size() << " validation_samples=" << report.errors.size()
            << "\nmax_absolute_error_V=" << report.max_absolute_error_V << "\nrmse_V=" << report.rmse_V
            << "\nnormalized_rmse=" << report.normalized_rmse << "\ntolerance_V=" << report.tolerance_V
            << "\nvalidation=PASSED\noutput=" << directory.string() << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return 1;
    }
}
