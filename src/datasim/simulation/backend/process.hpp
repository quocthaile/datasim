#pragma once
#include <filesystem>
#include <chrono>
#include <string>
#include <vector>
namespace datasim {
std::filesystem::path find_executable(const std::string& name);
int run_process(const std::filesystem::path& executable, const std::vector<std::string>& args,
                const std::filesystem::path& directory, const std::filesystem::path& log,
                std::chrono::milliseconds timeout = std::chrono::seconds(30));
}
