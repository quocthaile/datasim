#pragma once

#include "datasim/simulation/circuit/rc.hpp"
#include <array>
#include <filesystem>
#include <string>

namespace datasim_viewer {

inline constexpr std::array<const char*, 11> rc_field_names{
    "resistance_ohm", "capacitance_F", "voltage_V", "initial_voltage_V",
    "delay_s", "rise_s", "fall_s", "width_s", "period_s", "step_s", "duration_s"};

// Strict decimal/scientific notation using '.' as the decimal point. No whitespace,
// grouping separators, suffixes, unit conversion, NaN, or infinity are accepted.
[[nodiscard]] double parse_decimal(const std::string& text);
[[nodiscard]] datasim::RcConfig parse_rc_config(const std::array<std::string, 11>& values);

// Selects a fresh, not-yet-created child path. execute_rc_run() remains responsible
// for exclusive creation of the final directory.
[[nodiscard]] std::filesystem::path fresh_run_directory(const std::filesystem::path& parent);

} // namespace datasim_viewer
