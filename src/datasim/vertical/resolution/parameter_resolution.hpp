#pragma once
#include "datasim/component/evidence/evidence_location.hpp"
#include "datasim/component/evidence/source_document.hpp"
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/identity/ids.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include "datasim/vertical/validation/vertical_validation.hpp"
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace datasim::vertical {

// Traceable resolved component parameter preserving component identity, knowledge version,
// model version, NormalizedParameterId, ObservationId, source evidence, conversion rule,
// and operating conditions.
struct ResolvedComponentParameter {
    ComponentInstanceId instance_id;
    CircuitElementRole role = CircuitElementRole::unspecified;
    component::ComponentIdentity component;
    component::ComponentKnowledgeId knowledge_id;
    std::uint32_t knowledge_version = 1;
    component::ModelArtifactId model_id;
    std::string model_version;
    std::string model_parameter_name;
    component::NormalizedParameterId normalized_parameter_id;
    std::string canonical_name;
    component::ValueQualifier qualifier = component::ValueQualifier::unspecified;
    component::ParameterCategory category = component::ParameterCategory::other_unspecified;
    double resolved_si_value = 0.0;
    std::string normalized_si_unit;
    component::ObservationId source_observation_id;
    std::string preserved_original_value_text;
    std::string preserved_original_unit_symbol;
    std::string conversion_rule;
    std::optional<std::string> model_mapping_expression;
    std::vector<component::OperatingCondition> operating_conditions;
    std::vector<component::EvidenceLocation> source_evidence_locations;
    std::vector<component::SourceDocument> source_documents;
    std::vector<TerminalNodeBinding> terminal_bindings;

    auto operator<=>(const ResolvedComponentParameter&) const = default;
};

struct ResolvedRcParameters {
    ResolvedComponentParameter resistor;
    ResolvedComponentParameter capacitor;

    auto operator<=>(const ResolvedRcParameters&) const = default;
};

struct ParameterResolutionResult {
    VerticalValidationReport validation;
    std::optional<ResolvedRcParameters> parameters;
};

[[nodiscard]] ParameterResolutionResult resolve_rc_parameters(const CircuitAssembly& assembly);

} // namespace datasim::vertical
