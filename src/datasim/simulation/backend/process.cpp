#include "process.hpp"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace datasim {
namespace fs = std::filesystem;
fs::path find_executable(const std::string& name) {
    if (name.empty()) return {};
    auto exists = [](fs::path p) -> fs::path {
#ifdef _WIN32
        if (!p.has_extension()) p += ".exe";
#endif
        if (!fs::is_regular_file(p)) return {};
#ifndef _WIN32
        if (access(p.c_str(), X_OK) != 0) return {};
#endif
        return fs::absolute(p);
    };
    fs::path p(name);
    if (p.has_parent_path()) return exists(p);
    const char* env = std::getenv("PATH");
    std::istringstream paths(env ? env : ""); std::string dir;
#ifdef _WIN32
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif
    while (std::getline(paths, dir, separator)) {
        if (dir.empty()) continue;
        if (auto found = exists(fs::path(dir) / p); !found.empty()) return found;
    }
    return {};
}
int run_process(const fs::path& exe, const std::vector<std::string>& args,
                const fs::path& directory, const fs::path& log) {
#ifdef _WIN32
    // Quote using Windows argv rules; no command shell or command substitution.
    auto quote = [](const std::wstring& s) {
        std::wstring result = L"\""; unsigned slashes = 0;
        for (wchar_t c : s) {
            if (c == L'\\') { ++slashes; continue; }
            result.append(c == L'"' ? 2 * slashes + 1 : slashes, L'\\');
            result += c; slashes = 0;
        }
        result.append(2 * slashes, L'\\'); return result + L"\"";
    };
    std::wstring command = quote(exe.wstring());
    for (const auto& a : args) command += L" " + quote(fs::path(a).wstring());
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE output = CreateFileW(log.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create process log");
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = output; si.hStdError = output; si.hStdInput = input;
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, directory.c_str(), &si, &pi);
    CloseHandle(output); if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (!ok) throw std::runtime_error("process launch failed: Windows error " + std::to_string(GetLastError()));
    const DWORD wait = WaitForSingleObject(pi.hProcess, 30000);
    if (wait != WAIT_OBJECT_0) {
        TerminateProcess(pi.hProcess, 124); WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        throw std::runtime_error("backend timeout or wait failure (30 s)");
    }
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return static_cast<int>(code);
#else
    std::vector<std::string> storage{exe.string()};
    storage.insert(storage.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (auto& s : storage) argv.push_back(s.data());
    argv.push_back(nullptr);
    const pid_t pid = fork();
    if (pid < 0) throw std::runtime_error("fork failed");
    if (pid == 0) {
        const int out = open(log.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        const int in = open("/dev/null", O_RDONLY);
        if (out < 0 || in < 0 || chdir(directory.c_str()) ||
            dup2(out, STDOUT_FILENO) < 0 || dup2(out, STDERR_FILENO) < 0 || dup2(in, STDIN_FILENO) < 0) _exit(126);
        close(out); close(in); execv(exe.c_str(), argv.data()); _exit(127);
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    int status = 0;
    while (true) {
        const auto waited = waitpid(pid, &status, WNOHANG);
        if (waited == pid) break;
        if (waited < 0 && errno != EINTR) throw std::runtime_error("waitpid failed");
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(pid, SIGKILL); waitpid(pid, &status, 0); throw std::runtime_error("backend timeout (30 s)");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
#endif
}
}
