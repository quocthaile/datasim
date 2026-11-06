#include "process_capture.hpp"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>
#include <atomic>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace datasim::pdf::detail {
namespace {
#ifdef _WIN32
std::wstring widen(const std::string& text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                          static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) throw std::runtime_error("invalid UTF-8 process argument");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), count);
    return result;
}

std::wstring quote_windows(const std::wstring& value) {
    std::wstring result = L"\"";
    std::size_t slashes = 0;
    for (const wchar_t character : value) {
        if (character == L'\\') { ++slashes; continue; }
        if (character == L'"') {
            result.append(slashes * 2U + 1U, L'\\');
            result.push_back(L'"');
            slashes = 0;
            continue;
        }
        result.append(slashes, L'\\');
        slashes = 0;
        result.push_back(character);
    }
    result.append(slashes * 2U, L'\\');
    result.push_back(L'"');
    return result;
}
#endif
}

int run_capture(const std::filesystem::path& executable,
                const std::vector<std::string>& arguments,
                const std::filesystem::path& output_path,
                std::uintmax_t output_limit,
                const std::filesystem::path& monitored_output,
                std::chrono::milliseconds process_timeout) {
    constexpr auto poll_interval = std::chrono::milliseconds(10);
#ifdef _WIN32
    std::wstring command = quote_windows(executable.wstring());
    for (const auto& argument : arguments) command += L" " + quote_windows(widen(argument));
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("cannot create process output file");
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE pipe_read = nullptr, pipe_write = nullptr;
    if (!CreatePipe(&pipe_read, &pipe_write, &security, 0) ||
        !SetHandleInformation(pipe_read, HANDLE_FLAG_INHERIT, 0)) {
        if (pipe_read) CloseHandle(pipe_read);
        if (pipe_write) CloseHandle(pipe_write);
        throw std::runtime_error("cannot create bounded PDF backend output pipe");
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (!job) { CloseHandle(pipe_read); CloseHandle(pipe_write); throw std::runtime_error("cannot create PDF backend process job"); }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
    job_limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &job_limits, sizeof(job_limits))) {
        CloseHandle(job); CloseHandle(pipe_read); CloseHandle(pipe_write);
        throw std::runtime_error("cannot configure PDF backend process job");
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = pipe_write;
    startup.hStdError = pipe_write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &startup, &process);
    CloseHandle(pipe_write);
    if (!created) { CloseHandle(pipe_read); CloseHandle(job); throw std::runtime_error("cannot start PDF backend executable"); }
    if (!AssignProcessToJobObject(job, process.hProcess)) {
        TerminateProcess(process.hProcess, 125U);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job); CloseHandle(pipe_read);
        throw std::runtime_error("cannot contain PDF backend process tree");
    }
    if (ResumeThread(process.hThread) == static_cast<DWORD>(-1)) {
        TerminateJobObject(job, 125U);
        WaitForSingleObject(process.hProcess, INFINITE);
        CloseHandle(process.hThread); CloseHandle(process.hProcess); CloseHandle(job); CloseHandle(pipe_read);
        throw std::runtime_error("cannot resume PDF backend process");
    }
    std::atomic<bool> output_exceeded{false};
    std::thread output_reader([&] {
        char buffer[8192];
        std::uintmax_t written = 0;
        DWORD received = 0;
        while (ReadFile(pipe_read, buffer, sizeof(buffer), &received, nullptr) && received != 0) {
            const auto available = written < output_limit ? output_limit - written : 0U;
            const auto keep = static_cast<std::size_t>(std::min<std::uintmax_t>(received, available));
            if (keep != 0U) { output.write(buffer, static_cast<std::streamsize>(keep)); written += keep; }
            if (keep < received && !output_exceeded.exchange(true)) TerminateJobObject(job, 122U);
        }
        output.flush();
    });
    const auto started = std::chrono::steady_clock::now();
    DWORD status = 1;
    bool finished = false;
    while (!finished) {
        const auto wait_result = WaitForSingleObject(process.hProcess, 0U);
        if (wait_result == WAIT_OBJECT_0) {
            finished = true;
            if (!GetExitCodeProcess(process.hProcess, &status)) status = 125U;
            std::error_code log_ec, data_ec;
            const auto log_size = std::filesystem::file_size(output_path, log_ec);
            const auto data_size = monitored_output.empty() ? 0U : std::filesystem::file_size(monitored_output, data_ec);
            if ((!log_ec && log_size > output_limit) ||
                (!monitored_output.empty() && !data_ec && data_size > output_limit)) status = 122U;
            // The operation owns the job. Do not let descendants retain the
            // inherited output pipe after the primary backend has exited.
            (void)TerminateJobObject(job, 0U);
            break;
        }
        if (wait_result == WAIT_FAILED) {
            TerminateJobObject(job, 125U);
            WaitForSingleObject(process.hProcess, INFINITE);
            status = 125U;
            break;
        }
        if (std::chrono::steady_clock::now() - started >= process_timeout) {
            TerminateJobObject(job, 124U);
            WaitForSingleObject(process.hProcess, INFINITE);
            status = 124U;
            break;
        }
        std::this_thread::sleep_for(poll_interval);
    }
    output_reader.join();
    if (output_exceeded.load()) status = 122U;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(job);
    CloseHandle(pipe_read);
    return static_cast<int>(status);
#else
    // Prepare all argv storage before fork so the child performs only
    // async-signal-safe setup calls before execv when callers are concurrent.
    std::vector<std::string> storage{executable.string()};
    storage.insert(storage.end(), arguments.begin(), arguments.end());
    std::vector<char*> argv;
    argv.reserve(storage.size() + 1U);
    for (auto& value : storage) argv.push_back(value.data());
    argv.push_back(nullptr);
    const pid_t child = fork();
    if (child < 0) throw std::runtime_error("cannot fork PDF backend process");
    if (child == 0) {
        if (setpgid(0, 0) != 0) _exit(126);
        const rlimit file_limit{static_cast<rlim_t>(output_limit), static_cast<rlim_t>(output_limit)};
        (void)setrlimit(RLIMIT_FSIZE, &file_limit);
        const int output = open(output_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (output < 0 || dup2(output, STDOUT_FILENO) < 0 || dup2(output, STDERR_FILENO) < 0) _exit(126);
        close(output);
        execv(executable.c_str(), argv.data());
        _exit(127);
    }
    const int group_result = setpgid(child, child);
    const bool owns_group = group_result == 0 || getpgid(child) == child;
    const auto started = std::chrono::steady_clock::now();
    int status = 0;
    for (;;) {
        const pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        if (waited < 0 && errno != EINTR) {
            if (owns_group) (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            throw std::runtime_error("cannot wait for PDF backend");
        }
        std::error_code log_ec;
        std::error_code data_ec;
        const auto log_size = std::filesystem::file_size(output_path, log_ec);
        const auto data_size = monitored_output.empty() ? 0U : std::filesystem::file_size(monitored_output, data_ec);
        if ((!log_ec && log_size > output_limit) || (!monitored_output.empty() && !data_ec && data_size > output_limit)) {
            if (owns_group) (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            return 122;
        }
        if (std::chrono::steady_clock::now() - started >= process_timeout) {
            if (owns_group) (void)kill(-child, SIGKILL);
            (void)kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
            return 124;
        }
        std::this_thread::sleep_for(poll_interval);
    }
    if (WIFSIGNALED(status) && WTERMSIG(status) == SIGXFSZ) return output_limit_status;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#endif
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read PDF backend output");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace datasim::pdf::detail
