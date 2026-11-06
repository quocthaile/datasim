#include "read_only_samples.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

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
        std::cout << "PASS: desktop sample loading (valid, invalid, non-finite, missing, read-only)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
