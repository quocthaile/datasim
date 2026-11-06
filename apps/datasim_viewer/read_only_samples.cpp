#include "read_only_samples.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace datasim_viewer {

LoadedSamples load_samples_csv(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open results file for reading: " + path.string());
    }
    try {
        return {datasim::read_csv(input), path};
    } catch (const std::exception& error) {
        throw std::runtime_error("Invalid results CSV: " + std::string(error.what()));
    }
}

} // namespace datasim_viewer
