#pragma once
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace datasim::pdf::detail {
inline constexpr int process_timeout_status = 124;
inline constexpr int output_limit_status = 122;
// Throws before launch when the executable is missing or not executable.
// Returns the launched process exit status (or the timeout/output-limit status).
int run_capture(const std::filesystem::path& executable,
                const std::vector<std::string>& arguments,
                const std::filesystem::path& output_path,
                std::uintmax_t output_limit = 1024U * 1024U,
                const std::filesystem::path& monitored_output = {},
                std::chrono::milliseconds process_timeout = std::chrono::seconds(30));
std::string read_text_file(const std::filesystem::path& path);
}
