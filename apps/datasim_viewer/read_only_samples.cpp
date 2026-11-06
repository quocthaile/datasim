#include "read_only_samples.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <iterator>
#include <cstdint>
#include <algorithm>

namespace datasim_viewer {

LoadedSamples load_samples_csv(const std::filesystem::path& path) {
    // A 16 MiB bounded input buffer caps parser input; a separate row cap keeps
    // the resulting vector and GUI data set at or below one million samples.
    constexpr std::uintmax_t max_csv_bytes = 16U * 1024U * 1024U;
    constexpr std::size_t max_samples = 1000000U;
    std::ifstream input(path, std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open results file for reading: " + path.string());
    }
    try {
        std::string contents;
        contents.reserve(static_cast<std::size_t>(std::min<std::uintmax_t>(
            std::filesystem::file_size(path), max_csv_bytes)));
        char buffer[8192];
        while (input) {
            input.read(buffer, sizeof(buffer));
            const auto count = static_cast<std::size_t>(input.gcount());
            if (contents.size() + count > max_csv_bytes)
                throw std::runtime_error("CSV exceeds the 16 MiB viewer input limit");
            contents.append(buffer, count);
        }
        std::size_t lines = 0;
        for (const char ch : contents) if (ch == '\n') ++lines;
        if (!contents.empty() && contents.back() != '\n') ++lines;
        if (lines > max_samples + 1U)
            throw std::runtime_error("CSV exceeds the 1,000,000 sample viewer limit");
        std::istringstream parsed(contents);
        auto samples = datasim::read_csv(parsed);
        if (samples.size() > max_samples)
            throw std::runtime_error("CSV exceeds the 1,000,000 sample viewer limit");
        return {std::move(samples), path};
    } catch (const std::exception& error) {
        throw std::runtime_error("Invalid results CSV: " + std::string(error.what()));
    }
}

} // namespace datasim_viewer
