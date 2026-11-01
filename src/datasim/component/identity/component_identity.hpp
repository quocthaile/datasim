#pragma once
#include "datasim/component/identity/ids.hpp"
#include <compare>
#include <optional>
#include <string>

namespace datasim::component {

enum class ComponentFamily {
    resistor,
    capacitor,
    inductor,
    other
};

// Preserves exact manufacturer designation and suffixes without silent normalization.
struct ComponentIdentity {
    ComponentId id;
    std::string manufacturer;
    std::string part_number;
    std::optional<std::string> part_number_suffix;
    std::optional<std::string> package_id;
    ComponentFamily family = ComponentFamily::other;
    std::optional<std::string> subtype;
    bool is_synthetic = false;

    auto operator<=>(const ComponentIdentity&) const = default;
};

} // namespace datasim::component
