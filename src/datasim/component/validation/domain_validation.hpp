#pragma once
#include "datasim/component/evidence/evidence_location.hpp"
#include "datasim/component/evidence/source_document.hpp"
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/model/model_artifact.hpp"
#include "datasim/component/model/model_validation.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/component/observation/review.hpp"
#include <compare>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace datasim::component {

enum class ValidationRuleCode {
    empty_required_field,
    invalid_identifier,
    duplicate_identifier,
    unresolved_reference,
    non_finite_numeric_value,
    missing_numeric_source_text,
    missing_required_unit,
    contradictory_unitless_flag,
    invalid_date_or_timestamp,
    invalid_sha256_digest,
    empty_evidence_locator,
    missing_evidence_reference,
    invalid_review_transition,
    invalid_review_history,
    unauthorized_reviewer_kind,
    self_approval_prohibited,
    missing_superseding_reference,
    unexpected_superseding_reference,
    invalid_superseding_target,
    missing_value_cannot_be_approved,
    unapproved_observation_in_knowledge,
    category_mismatch,
    qualifier_mismatch,
    original_notation_mismatch,
    reversed_qualifier_bounds,
    undeclared_parameter_conflict,
    invalid_conflict_record,
    conflicted_observation_promoted,
    invalid_version_history,
    synthetic_provenance_mismatch,
    absolute_maximum_misused_as_model_parameter,
    invalid_terminal_mapping,
    invalid_model_version,
    missing_model_source_or_license,
    missing_artifact_reference,
    invalid_parameter_mapping,
    invalid_validity_envelope,
    empty_supported_analyses,
    empty_model_limitations,
    invalid_model_validation_record,
    incompatible_parameter_units,
    incompatible_source_document_part_number
};

struct ValidationIssue {
    ValidationRuleCode code;
    std::string entity_path;
    std::string message;

    auto operator<=>(const ValidationIssue&) const = default;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] bool valid() const noexcept { return issues.empty(); }

    [[nodiscard]] bool has_code(ValidationRuleCode code) const noexcept {
        for (const auto& issue : issues) {
            if (issue.code == code) {
                return true;
            }
        }
        return false;
    }

    auto operator<=>(const ValidationReport&) const = default;
};

// Cohesive in-memory aggregate of all domain records associated with one component.
struct ComponentDomainBundle {
    ComponentIdentity component;
    std::vector<SourceDocument> documents;
    std::vector<EvidenceLocation> evidence_locations;
    std::vector<ParameterObservation> observations;
    std::optional<ComponentKnowledge> knowledge;
    std::vector<ModelArtifactDescriptor> models;
    std::vector<ModelValidationRecord> model_validations;

    auto operator<=>(const ComponentDomainBundle&) const = default;
};

[[nodiscard]] bool is_valid_identifier_token(std::string_view token) noexcept;
[[nodiscard]] bool is_valid_version_token(std::string_view token) noexcept;
[[nodiscard]] bool is_valid_sha256_hex(std::string_view digest) noexcept;
[[nodiscard]] bool is_valid_iso8601_date_or_timestamp(std::string_view text) noexcept;

[[nodiscard]] bool are_parameter_units_compatible(
    std::string_view expected_unit,
    const NormalizedParameter& param,
    std::optional<std::string_view> mapping_expression = std::nullopt) noexcept;

[[nodiscard]] bool is_source_document_applicable(
    const SourceDocument& document,
    const ComponentIdentity& component) noexcept;

[[nodiscard]] ValidationReport validate_component_identity(const ComponentIdentity& identity);
[[nodiscard]] ValidationReport validate_source_document(const SourceDocument& document);
[[nodiscard]] ValidationReport validate_evidence_location(const EvidenceLocation& location);
[[nodiscard]] ValidationReport validate_operating_condition(
    const OperatingCondition& condition,
    std::string_view path_prefix = "operating_condition");
[[nodiscard]] ValidationReport validate_review_decision(
    const ReviewDecision& decision,
    std::string_view path_prefix = "review_decision");
[[nodiscard]] ValidationReport validate_parameter_observation(const ParameterObservation& observation);
[[nodiscard]] ValidationReport validate_normalized_parameter(const NormalizedParameter& parameter);
[[nodiscard]] ValidationReport validate_component_knowledge(const ComponentKnowledge& knowledge);
[[nodiscard]] ValidationReport validate_model_parameter_mapping(
    const ModelParameterMapping& mapping,
    const NormalizedParameter* resolved_param = nullptr,
    std::string_view path_prefix = "parameter_mapping");
[[nodiscard]] ValidationReport validate_model_artifact_descriptor(const ModelArtifactDescriptor& descriptor);
[[nodiscard]] ValidationReport validate_model_validation_record(const ModelValidationRecord& record);

// Checks structural validity, ID uniqueness, and cross-entity referential integrity.
[[nodiscard]] ValidationReport validate_domain_bundle(const ComponentDomainBundle& bundle);

// Throws std::invalid_argument summarizing the first violated rule when !report.valid().
void ensure_valid(const ValidationReport& report);

} // namespace datasim::component
