#pragma once
#include <compare>
#include <stdexcept>
#include <string>
#include <vector>

namespace datasim::vertical {

enum class VerticalValidationCode {
    empty_required_field,
    invalid_identifier,
    duplicate_identifier,
    unsupported_circuit_topology,
    invalid_circuit_nodes,
    invalid_component_bundle,
    unresolved_component_reference,
    unresolved_knowledge_reference,
    knowledge_version_mismatch,
    unapproved_component_knowledge,
    unresolved_knowledge_conflicts,
    unresolved_model_reference,
    model_version_mismatch,
    model_knowledge_mismatch,
    invalid_topology_element_roles,
    component_family_role_mismatch,
    invalid_terminal_node_binding,
    shorted_component_terminals,
    topology_connectivity_mismatch,
    missing_model_parameter_mapping,
    conflicting_model_parameter_mappings,
    unsupported_model_parameter,
    unresolved_normalized_parameter,
    conflicting_normalized_parameters,
    non_typical_parameter_qualifier,
    invalid_parameter_category,
    missing_or_invalid_si_value,
    non_si_or_incompatible_parameter_unit,
    unsupported_unit_mapping,
    missing_conversion_rule,
    unresolved_source_observation,
    unapproved_source_observation,
    unresolved_source_evidence,
    unresolved_source_document,
    unsupported_model_format,
    external_artifact_unavailable_in_rc_backend,
    unsupported_backend_analysis,
    analysis_not_supported_by_model,
    model_unvalidated,
    model_failed_validation,
    model_validation_out_of_date,
    analysis_not_verified_in_validation_record,
    invalid_operating_condition,
    operating_condition_outside_validity_envelope,
    synthetic_data_prohibited_in_engineering_mode,
    inconsistent_synthetic_classification,
    unsupported_solver_controls,
    invalid_rc_simulation_configuration
};

struct VerticalValidationIssue {
    VerticalValidationCode code;
    std::string entity_path;
    std::string message;

    auto operator<=>(const VerticalValidationIssue&) const = default;
};

struct VerticalValidationReport {
    std::vector<VerticalValidationIssue> issues;

    [[nodiscard]] bool valid() const noexcept { return issues.empty(); }

    [[nodiscard]] bool has_code(VerticalValidationCode code) const noexcept {
        for (const auto& issue : issues) {
            if (issue.code == code) {
                return true;
            }
        }
        return false;
    }

    void add(VerticalValidationCode code, std::string path, std::string message) {
        issues.push_back(VerticalValidationIssue{code, std::move(path), std::move(message)});
    }

    void append(const VerticalValidationReport& other) {
        issues.insert(issues.end(), other.issues.begin(), other.issues.end());
    }

    auto operator<=>(const VerticalValidationReport&) const = default;
};

inline void ensure_vertical_valid(const VerticalValidationReport& report) {
    if (!report.valid()) {
        const auto& first = report.issues.front();
        throw std::invalid_argument(first.entity_path + ": " + first.message);
    }
}

} // namespace datasim::vertical

