#pragma once
#include "datasim/component/identity/ids.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/component/observation/review.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

enum class ValueQualifier {
    minimum,
    typical,
    maximum,
    unspecified
};

enum class ParameterCategory {
    absolute_maximum_rating,
    recommended_operating_condition,
    electrical_characteristic,
    thermal_characteristic,
    other_unspecified
};

enum class ExtractionMethod {
    manual_entry,
    synthetic_fixture,
    deterministic_table_parser,
    deterministic_text_parser,
    curve_digitization,
    ocr,
    llm_assisted
};

// Single qualified reading from one or more source locations. Missing cells remain
// std::nullopt rather than zero; min/typ/max values are never merged into one record.
struct ParameterObservation {
    ObservationId id;
    ComponentId component_id;
    std::string parameter_name;
    std::optional<std::string> original_value_text;
    std::optional<std::string> original_unit_symbol;
    bool unitless = false;
    std::optional<double> parsed_numeric_value;
    ValueQualifier qualifier = ValueQualifier::unspecified;
    ParameterCategory category = ParameterCategory::other_unspecified;
    std::vector<OperatingCondition> operating_conditions;
    std::vector<EvidenceId> evidence_refs;
    ExtractionMethod extraction_method = ExtractionMethod::manual_entry;
    std::optional<std::string> extraction_tool;
    std::optional<std::string> extraction_tool_version;
    std::optional<std::string> extracted_at;
    std::optional<std::string> uncertainty_note;
    ReviewState review_state = ReviewState::candidate;
    std::vector<ReviewDecision> review_history;
    std::optional<ObservationId> superseded_by;

    auto operator<=>(const ParameterObservation&) const = default;
};

} // namespace datasim::component
