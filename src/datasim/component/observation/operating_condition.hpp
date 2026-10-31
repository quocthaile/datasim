#pragma once
#include <compare>
#include <optional>
#include <string>

namespace datasim::component {

// Test or operating condition accompanying a datasheet parameter or normalized fact.
struct OperatingCondition {
    std::string parameter_name;
    std::string original_value_text;
    std::string unit_symbol;
    std::optional<double> numeric_value;
    std::optional<std::string> notes;

    auto operator<=>(const OperatingCondition&) const = default;
};

} // namespace datasim::component
