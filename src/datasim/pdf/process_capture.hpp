#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace datasim::pdf::detail {
int run_capture(const std::filesystem::path& executable,
                const std::vector<std::string>& arguments,
                const std::filesystem::path& output_path);
std::string read_text_file(const std::filesystem::path& path);
}
