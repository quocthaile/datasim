#pragma once

#include "datasim/simulation/io/samples.hpp"
#include <filesystem>

namespace datasim_viewer {

struct LoadedSamples {
    datasim::Samples samples;
    std::filesystem::path source_path;
};

// Opens an existing results.csv for reading and delegates all CSV validation to datasim.
[[nodiscard]] LoadedSamples load_samples_csv(const std::filesystem::path& path);

} // namespace datasim_viewer
