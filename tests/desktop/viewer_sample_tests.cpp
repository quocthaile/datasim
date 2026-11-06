#include "read_only_samples.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <chrono>

namespace fs = std::filesystem;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string read_bytes(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected isolated test output directory");
        const fs::path directory = fs::path(argv[1]) / "viewer-sample-tests";
        fs::create_directories(directory);
        const auto valid = directory / "valid.csv";
        const std::string valid_content = "time_s,vin_V,vout_V\n0,0,0\n0.001,5,2.1\n";
        { std::ofstream out(valid, std::ios::binary); out << valid_content; }
        const auto before = read_bytes(valid);
        const auto loaded = datasim_viewer::load_samples_csv(valid);
        const auto after = read_bytes(valid);
        require(loaded.samples.size() == 2, "valid CSV sample count was not loaded");
        require(loaded.samples.back().vout_V == 2.1, "valid CSV values were not preserved");
        require(before == after, "input CSV bytes changed during read-only load");
        const auto repeated = datasim_viewer::load_samples_csv(valid);
        require(repeated.samples.size() == loaded.samples.size() &&
                repeated.samples[1].time_s == loaded.samples[1].time_s &&
                repeated.samples[1].vin_V == loaded.samples[1].vin_V &&
                repeated.samples[1].vout_V == loaded.samples[1].vout_V,
                "repeated load changed parsed samples");

        const auto invalid = directory / "invalid.csv";
        { std::ofstream out(invalid, std::ios::binary); out << "time_s,vin_V,vout_V\n0,nan,0\n"; }
        bool invalid_rejected = false;
        try { (void)datasim_viewer::load_samples_csv(invalid); }
        catch (const std::runtime_error& error) {
            invalid_rejected = std::string(error.what()).find("Invalid results CSV:") == 0;
        }
        require(invalid_rejected, "invalid or non-finite CSV did not return a visible load error");

        bool missing_rejected = false;
        try { (void)datasim_viewer::load_samples_csv(directory / "missing.csv"); }
        catch (const std::runtime_error& error) {
            missing_rejected = std::string(error.what()).find("Cannot open results file") == 0;
        }
        require(missing_rejected, "missing file did not return a visible load error");
        const auto oversized = directory / "oversized.csv";
        { std::ofstream out(oversized, std::ios::binary); out << "time_s,vin_V,vout_V\n" << std::string(16U * 1024U * 1024U, 'x'); }
        bool oversized_rejected = false;
        try { (void)datasim_viewer::load_samples_csv(oversized); }
        catch (const std::runtime_error& error) {
            oversized_rejected = std::string(error.what()).find("16 MiB") != std::string::npos;
        }
        require(oversized_rejected, "CSV exceeding the input byte limit was not rejected");
        const auto maximum = directory / "maximum.csv";
        {
            std::ofstream out(maximum, std::ios::binary);
            out << "time_s,vin_V,vout_V\n";
            for (std::size_t i = 0; i < 1000000U; ++i) out << i << ",0,0\n";
        }
        const auto load_started = std::chrono::steady_clock::now();
        const auto maximum_loaded = datasim_viewer::load_samples_csv(maximum);
        const auto load_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - load_started).count();
        require(maximum_loaded.samples.size() == 1000000U,
                "maximum supported viewer CSV sample count changed");
        std::cout << "MEASURED: 1,000,000 CSV samples loaded in " << load_elapsed << " ms\n";
        std::cout << "PASS: desktop sample loading (valid, invalid, non-finite, missing, read-only)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
