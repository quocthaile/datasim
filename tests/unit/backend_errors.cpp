#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace datasim;
namespace fs = std::filesystem;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("fixture executable required");
        const auto base = fs::absolute("fault tests " + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directory(base);
        if (!find_executable((base / "missing.exe").string()).empty()) throw std::runtime_error("missing executable detection failed");
        bool launch_failed = false;
        try { run_process(base / "missing.exe", {}, base, base / "launch.log"); }
        catch (const std::exception&) { launch_failed = true; }
#ifdef _WIN32
        if (!launch_failed) throw std::runtime_error("launch failure not reported");
#else
        // POSIX exec failure is a nonzero child exit, checked by the backend.
        (void)launch_failed;
        if (run_process(base / "missing.exe", {}, base, base / "launch.log") == 0) throw std::runtime_error("exec failure hidden");
#endif
        for (const auto* mode : {"exit", "missing", "warning", "empty", "malformed", "nonfinite"}) {
            const auto dir = base / mode; fs::create_directory(dir);
            { std::ofstream out(dir / "mode.txt"); out << mode; }
            bool failed = false;
            try { run_ngspice(RcConfig{}, fs::absolute(argv[1]), dir); }
            catch (const std::exception& e) { failed = true; std::cout << mode << ": rejected: " << e.what() << '\n'; }
            if (!failed) throw std::runtime_error(std::string("fault not rejected: ") + mode);
        }
        std::cout << "PASS: missing executable, failed launch and six backend failure modes\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
