#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include "datasim/simulation/io/report.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <future>
#include <thread>
#include <iterator>
using namespace datasim;
namespace fs = std::filesystem;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("fixture executable required");
        const auto base = unique_workspace("fault tests");
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
        for (const auto* mode : {"exit", "missing", "warning", "empty", "malformed", "nonfinite", "signal"}) {
            const auto dir = base / mode; fs::create_directory(dir);
            { std::ofstream out(dir / "mode.txt"); out << mode; }
            bool failed = false;
            try { run_ngspice(RcConfig{}, fs::absolute(argv[1]), dir); }
            catch (const std::exception& e) { failed = true; std::cout << mode << ": rejected: " << e.what() << '\n'; }
            if (!failed) throw std::runtime_error(std::string("fault not rejected: ") + mode);
        }
        const auto timeout_dir=unique_workspace(base);
        bool timed_out=false; const auto started=std::chrono::steady_clock::now();
        try { run_process(fs::absolute(argv[1]),{"--hang"},timeout_dir,timeout_dir/"timeout.log",std::chrono::milliseconds(200)); }
        catch (const std::exception& e) { timed_out=std::string(e.what()).find("timeout")!=std::string::npos; }
        if (!timed_out || std::chrono::steady_clock::now()-started>std::chrono::seconds(3)) throw std::runtime_error("timeout did not terminate promptly");
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
        if (fs::exists(timeout_dir/"must-not-exist.txt")) throw std::runtime_error("timed-out child survived");
        std::vector<std::string> arguments{"--echo","path with spaces","quote\"inside","trailing\\","a;&|$(touch injected)",""};
        const auto invoke=[&] {
            const auto dir=unique_workspace(base);
            if (run_process(fs::absolute(argv[1]),arguments,dir,dir/"args.log")!=0) throw std::runtime_error("echo failed");
            std::ifstream in(dir/"args.log");
            std::string observed{std::istreambuf_iterator<char>(in),{}};
            std::string expected; for (std::size_t i=1;i<arguments.size();++i) expected+=arguments[i]+"\n";
            if (observed!=expected || !fs::exists(dir/"cwd-marker.txt")) throw std::runtime_error("argument or cwd mismatch");
            return dir;
        };
        auto a=std::async(std::launch::async,invoke), b=std::async(std::launch::async,invoke);
        if (a.get()==b.get()) throw std::runtime_error("concurrent workspace collision");
        const auto invalid_dir=base/"invalid"; RcConfig invalid; invalid.resistance_ohm=0;
        bool invalid_failed=false;
        try { execute_rc_run(invalid,argv[1],invalid_dir); } catch (const std::exception&) { invalid_failed=true; }
        if (!invalid_failed || fs::exists(invalid_dir)) throw std::runtime_error("invalid input touched workspace");
        const auto failed_dir=base/"missing-backend-run";
        try { execute_rc_run(RcConfig{},(base/"absent.exe").string(),failed_dir); } catch (const std::exception&) {}
        if (!fs::exists(failed_dir/"run_metadata.json") || fs::exists(failed_dir/"results.csv")) throw std::runtime_error("failed run artifacts invalid");
        std::ofstream(base/"user-sentinel.txt") << "must stay";
        bool existing_failed=false;
        try { execute_rc_run(RcConfig{},argv[1],base); } catch (const std::exception&) { existing_failed=true; }
        if (!existing_failed || !fs::exists(base/"user-sentinel.txt")) throw std::runtime_error("existing data not protected");
        std::cout << "PASS: backend faults, missing signal, timeout termination, concurrent workspaces, literal argv, cwd, retained logs, input gate and user data protection\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
