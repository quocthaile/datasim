#pragma once
#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include <filesystem>
#include <string>
namespace datasim {
std::string json_string(const std::string&);
std::string config_json(const RcConfig&);
std::string content_id(const std::string&); // FNV-1a-64, non-security identity.
struct RunResult { Samples samples; Validation validation; double backend_seconds; };
RunResult execute_rc_run(const RcConfig&, const std::string& executable, const std::filesystem::path& new_directory);
std::filesystem::path unique_workspace(const std::filesystem::path& parent);
}
