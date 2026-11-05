#include "process_capture.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
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
                const std::filesystem::path& output_path) {
#ifdef _WIN32
    std::wstring command = quote_windows(executable.wstring());
    for (const auto& argument : arguments) command += L" " + quote_windows(widen(argument));
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE output = CreateFileW(output_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                &security, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("cannot create process output file");
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output;
    startup.hStdError = output;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(output);
    if (!created) throw std::runtime_error("cannot start PDF backend executable");
    const DWORD wait_result = WaitForSingleObject(process.hProcess, 30000U);
    DWORD status = 1;
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 124U);
        WaitForSingleObject(process.hProcess, 5000U);
        status = 124U;
    } else if (wait_result != WAIT_OBJECT_0 || !GetExitCodeProcess(process.hProcess, &status)) {
        status = 125U;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(status);
#else
    const pid_t child = fork();
    if (child < 0) throw std::runtime_error("cannot fork PDF backend process");
    if (child == 0) {
        const int output = open(output_path.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0600);
        if (output < 0 || dup2(output, STDOUT_FILENO) < 0 || dup2(output, STDERR_FILENO) < 0) _exit(126);
        close(output);
        std::vector<std::string> storage{executable.string()};
        storage.insert(storage.end(), arguments.begin(), arguments.end());
        std::vector<char*> argv;
        for (auto& value : storage) argv.push_back(value.data());
        argv.push_back(nullptr);
        execv(executable.c_str(), argv.data());
        _exit(127);
    }
    int status = 0;
    if (waitpid(child, &status, 0) < 0) throw std::runtime_error("cannot wait for PDF backend");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
#endif
}

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read PDF backend output");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
} // namespace datasim::pdf::detail
