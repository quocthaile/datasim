#pragma once
#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/simulation/io/samples.hpp"
#include <filesystem>
#include <string>
namespace datasim {
std::string rc_netlist(const RcConfig&);
Samples read_ngspice_table(std::istream&);
std::string backend_version(const std::filesystem::path&, const std::filesystem::path& directory);
Samples run_ngspice(const RcConfig&, const std::filesystem::path& executable,
                    const std::filesystem::path& directory);
}
