#pragma once
#include "datasim/component/identity/ids.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/component/observation/parameter_observation.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

// Normalized SI representation derived from an approved observation via an explicit rule.
// Preserves original value text, unit symbol, qualifier, category, and observation reference.
struct NormalizedParameter {
    NormalizedParameterId id;
    std::string canonical_name;
    std::string quantity_description;
    ValueQualifier qualifier = ValueQualifier::unspecified;
    ParameterCategory category = ParameterCategory::other_unspecified;
    std::optional<double> normalized_numeric_value;
    std::optional<std::string> normalized_si_unit;
    bool unitless = false;
    ObservationId source_observation_id;
    std::string preserved_original_value_text;
    std::optional<std::string> preserved_original_unit_symbol;
    std::string conversion_rule;
    std::vector<OperatingCondition> applicable_conditions;

    auto operator<=>(const NormalizedParameter&) const = default;
};

// Constructs a NormalizedParameter from an approved observation using an explicit
// conversion rule, preserving original notation, unit, qualifier, and rating category.
NormalizedParameter make_normalized_parameter(
    NormalizedParameterId id,
    const ParameterObservation& approved_observation,
    std::string canonical_name,
    std::string quantity_description,
    std::optional<double> normalized_numeric_value,
    std::optional<std::string> normalized_si_unit,
    bool unitless,
    std::string conversion_rule,
    std::vector<OperatingCondition> applicable_conditions);

} // namespace datasim::component
