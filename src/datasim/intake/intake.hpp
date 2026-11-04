#pragma once

#include "datasim/component/validation/domain_validation.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace datasim::intake {

enum class IntakeOriginClass {
    synthetic_fixture,
    manual_curation
};

enum class IntakeDiagnosticSeverity {
    warning,
    review_required,
    error
};

enum class IntakeDiagnosticCode {
    invalid_schema_version,
    invalid_origin,
    invalid_curator,
    manufacturer_publisher_mismatch,
    part_number_suffix_mismatch,
    page_index_out_of_range,
    derived_page_index,
    inconsistent_synthetic_markers,
    synthetic_masquerading_as_manufacturer,
    invalid_intake_entry,
    ambiguous_numeric_text,
    asserted_ambiguous_numeric_value,
    raw_numeric_mismatch,
    unsupported_unit_conversion,
    qualifier_text_mismatch,
    category_section_mismatch,
    non_candidate_state_in_intake,
    non_empty_review_history_in_intake,
    superseded_observation_in_intake,
    conflict_candidate,
    unsupported_embedded_content
};

struct IntakeEvidenceEntry {
    component::EvidenceLocation location;
    std::optional<std::string> row_label;
    std::optional<std::string> column_label;
    bool pdf_page_index_was_derived = false;
};

struct IntakeObservationEntry {
    component::ParameterObservation observation;
    std::optional<std::string> raw_symbol;
    std::optional<std::string> raw_qualifier_text;
    std::optional<std::string> raw_conditions_text;
    std::optional<std::string> ambiguity_notes;
};

struct IntakeManifest {
    std::string schema_version = "1.0";
    IntakeOriginClass origin = IntakeOriginClass::manual_curation;
    component::ReviewerIdentity curator;
    component::ComponentIdentity component;
    component::SourceDocument source_document;
    std::optional<std::size_t> declared_pdf_page_count;
    std::optional<std::string> publisher_difference_declaration;
    std::vector<IntakeEvidenceEntry> evidence;
    std::vector<IntakeObservationEntry> observations;
};

struct IntakeDiagnostic {
    IntakeDiagnosticCode code;
    IntakeDiagnosticSeverity severity;
    std::string entity_path;
    std::string message;

    auto operator<=>(const IntakeDiagnostic&) const = default;
};

struct IntakeReport {
    component::ValidationReport domain_validation;
    std::vector<IntakeDiagnostic> diagnostics;

    [[nodiscard]] bool valid() const noexcept {
        if (!domain_validation.valid()) {
            return false;
        }
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.severity == IntakeDiagnosticSeverity::error) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool has_code(IntakeDiagnosticCode code) const noexcept {
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.code == code) {
                return true;
            }
        }
        return false;
    }
};

[[nodiscard]] IntakeReport validate_intake_manifest(const IntakeManifest& manifest);

struct CandidateBundleResult {
    IntakeReport report;
    std::optional<component::ComponentDomainBundle> bundle;
};

[[nodiscard]] CandidateBundleResult build_candidate_bundle(const IntakeManifest& manifest);

} // namespace datasim::intake
