#include "process_capture.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#include <thread>
#endif

namespace fs = std::filesystem;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("expected fixture executable and isolated directory");
        const fs::path fixture = fs::absolute(argv[1]);
        const fs::path root = fs::absolute(argv[2]);
        fs::create_directories(root);
        const auto exact = root / "exact.bin";
        require(datasim::pdf::detail::run_capture(fixture, {"--write", "1024"}, exact, 1024) == 0,
                "exact-limit process output should succeed");
        require(fs::file_size(exact) == 1024, "exact-limit output size changed");
        const auto excessive = root / "excessive.bin";
        require(datasim::pdf::detail::run_capture(fixture, {"--stderr", "1048576"}, excessive, 1024) != 0,
                "excessive stderr must fail");
        require(fs::file_size(excessive) == 1024, "stderr capture did not persist exactly the configured quota");
        const auto excessive_stdout = root / "excessive-stdout.bin";
        require(datasim::pdf::detail::run_capture(fixture, {"--write", "1048576"},
                    excessive_stdout, 1024) != 0,
                "excessive stdout must fail");
        require(fs::file_size(excessive_stdout) == 1024,
                "stdout capture did not persist exactly the configured quota");
        const auto failed = root / "failed.bin";
        require(datasim::pdf::detail::run_capture(fixture, {"--exit"}, failed) == 9,
                "normal nonzero exit code was not preserved");
        bool missing_rejected = false;
        try { (void)datasim::pdf::detail::run_capture(root / "missing-executable", {}, root / "missing.log"); }
        catch (const std::exception&) { missing_rejected = true; }
        require(missing_rejected, "missing executable was not reported");
#ifndef _WIN32
        const auto non_executable = root / "non-executable";
        { std::ofstream file(non_executable); file << "not executable\n"; }
        fs::permissions(non_executable, fs::perms::owner_read | fs::perms::owner_write,
                        fs::perm_options::replace);
        bool permission_rejected = false;
        try { (void)datasim::pdf::detail::run_capture(non_executable, {}, root / "non-executable.log"); }
        catch (const std::exception&) { permission_rejected = true; }
        require(permission_rejected, "non-executable file was not reported before launch");
#endif
        const auto started = std::chrono::steady_clock::now();
        require(datasim::pdf::detail::run_capture(fixture, {"--hang"}, root / "hang.log", 1024, {},
                    std::chrono::milliseconds(150)) == 124, "hung child did not report timeout");
        require(std::chrono::steady_clock::now() - started < std::chrono::seconds(2),
                "hung child exceeded the bounded timeout allowance");
#ifndef _WIN32
        const auto escaped_marker = root / "escaped-child.txt";
        require(datasim::pdf::detail::run_capture(fixture, {"--spawn-child", escaped_marker.string()},
                    root / "parent-child.log", 1024, {}, std::chrono::milliseconds(150)) == 124,
                "parent and child did not hit the bounded process timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        require(!fs::exists(escaped_marker), "timed-out descendant escaped process-group cleanup");
#endif
        std::vector<std::thread> callers;
        std::vector<int> outcomes(4, -1);
        for (std::size_t i = 0; i < outcomes.size(); ++i) callers.emplace_back([&, i] {
            outcomes[i] = datasim::pdf::detail::run_capture(fixture, {"--write", "256"},
                root / ("parallel-" + std::to_string(i) + ".bin"), 1024);
        });
        for (auto& caller : callers) caller.join();
        for (const auto status : outcomes) require(status == 0, "concurrent process capture interfered");
        std::cout << "PASS: PDF process exit, missing executable, runtime quota, timeout and concurrent isolation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
