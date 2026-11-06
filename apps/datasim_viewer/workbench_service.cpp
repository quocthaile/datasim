#include "workbench_service.hpp"

#include "datasim/simulation/io/report.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <system_error>

namespace datasim_viewer {

double parse_decimal(const std::string& text) {
    if (text.empty()) throw std::invalid_argument("value is empty");
    double value = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value,
                                        std::chars_format::general);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(value))
        throw std::invalid_argument("use a finite decimal or scientific value with '.' and no unit suffix");
    return value;
}

datasim::RcConfig parse_rc_config(const std::array<std::string, 11>& values) {
    datasim::RcConfig config;
    std::array<double, 11> parsed{};
    for (std::size_t i = 0; i < values.size(); ++i) {
        try { parsed[i] = parse_decimal(values[i]); }
        catch (const std::exception& error) {
            throw std::invalid_argument(std::string(rc_field_names[i]) + ": " + error.what());
        }
    }
    config.resistance_ohm = parsed[0];
    config.capacitance_F = parsed[1];
    config.voltage_V = parsed[2];
    config.initial_voltage_V = parsed[3];
    config.delay_s = parsed[4];
    config.rise_s = parsed[5];
    config.fall_s = parsed[6];
    config.width_s = parsed[7];
    config.period_s = parsed[8];
    config.step_s = parsed[9];
    config.duration_s = parsed[10];
    datasim::check_config(config);
    return config;
}

std::filesystem::path fresh_run_directory(const std::filesystem::path& parent) {
    namespace fs = std::filesystem;
    std::error_code error;
    if (!fs::is_directory(parent, error) || error)
        throw std::invalid_argument("output parent must be an existing directory");
    static std::atomic_uint64_t sequence{0};
    const auto ticks = std::chrono::system_clock::now().time_since_epoch().count();
    const auto absolute_parent = fs::absolute(parent, error);
    if (error) throw std::runtime_error("cannot resolve output parent: " + error.message());
    const auto leaf = "manual-ideal-rc-" + std::to_string(ticks) + "-" +
                      std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    const auto candidate = absolute_parent / leaf;
    if (fs::exists(candidate, error) || error)
        throw std::runtime_error("run output path collision: " + candidate.string());
    return candidate;
}

} // namespace datasim_viewer
