#include "datasim/intake/validation/intake_validation.hpp"
#include "datasim/intake/numeric/raw_numeric.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <string>
#include <utility>

namespace datasim::intake {
namespace {

using component::ParameterCategory;
using component::ValueQualifier;

void add(
    IntakeReport& report,
    IntakeDiagnosticCode code,
    IntakeDiagnosticSeverity severity,
    std::string path,
    std::string message) {
    report.diagnostics.push_back({code, severity, std::move(path), std::move(message)});
}

bool ends_with(const std::string& value, const std::string& suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool qualifier_matches(std::string text, ValueQualifier qualifier) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (text == "min" || text == "minimum") return qualifier == ValueQualifier::minimum;
    if (text == "typ" || text == "typical") return qualifier == ValueQualifier::typical;
    if (text == "max" || text == "maximum") return qualifier == ValueQualifier::maximum;
    return true;
}

bool section_suggests_absolute_maximum(const IntakeEvidenceEntry& entry) {
    if (!entry.location.section.has_value()) return false;
    std::string section = *entry.location.section;
    std::transform(section.begin(), section.end(), section.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return section.find("absolute maximum") != std::string::npos;
}

} // namespace

IntakeReport validate_intake_manifest(const IntakeManifest& manifest) {
    IntakeReport report;
    if (manifest.schema_version != "1.0") {
        add(report, IntakeDiagnosticCode::invalid_schema_version,
            IntakeDiagnosticSeverity::error, "schema_version", "unsupported intake schema version");
    }
    if (manifest.curator.id.empty()) {
        add(report, IntakeDiagnosticCode::invalid_curator,
            IntakeDiagnosticSeverity::error, "curator.id", "curator identity must be present");
    }
    if (manifest.declared_pdf_page_count.has_value() &&
        *manifest.declared_pdf_page_count == 0) {
        add(report, IntakeDiagnosticCode::invalid_intake_entry,
            IntakeDiagnosticSeverity::error, "declared_pdf_page_count",
            "declared PDF page count must be greater than zero");
    }
    if (manifest.publisher_difference_declaration.has_value() &&
        manifest.publisher_difference_declaration->empty()) {
        add(report, IntakeDiagnosticCode::invalid_intake_entry,
            IntakeDiagnosticSeverity::error, "publisher_difference_declaration",
            "publisher difference declaration must not be blank");
    }
    if (manifest.origin == IntakeOriginClass::synthetic_fixture) {
        if (!manifest.component.is_synthetic || !manifest.source_document.is_synthetic) {
            add(report, IntakeDiagnosticCode::inconsistent_synthetic_markers,
                IntakeDiagnosticSeverity::error, "origin",
                "synthetic_fixture requires synthetic component and source document");
        }
    } else if (manifest.component.is_synthetic || manifest.source_document.is_synthetic) {
        add(report, IntakeDiagnosticCode::inconsistent_synthetic_markers,
            IntakeDiagnosticSeverity::error, "origin",
            "manual_curation cannot contain synthetic component or source document markers");
    }

    report.domain_validation = component::validate_component_identity(manifest.component);
    const auto document_report = component::validate_source_document(manifest.source_document);
    report.domain_validation.issues.insert(
        report.domain_validation.issues.end(),
        document_report.issues.begin(),
        document_report.issues.end());
    if (manifest.source_document.publisher != manifest.component.manufacturer &&
        !manifest.publisher_difference_declaration.has_value()) {
        add(report, IntakeDiagnosticCode::manufacturer_publisher_mismatch,
            IntakeDiagnosticSeverity::error, "source_document.publisher",
            "publisher must exactly match component manufacturer unless a declaration is supplied");
    }
    if (std::find(
            manifest.source_document.associated_part_numbers.begin(),
            manifest.source_document.associated_part_numbers.end(),
            manifest.component.part_number) ==
        manifest.source_document.associated_part_numbers.end()) {
        report.domain_validation.issues.push_back({
            component::ValidationRuleCode::incompatible_source_document_part_number,
            "source_document.associated_part_numbers",
            "source document does not contain the exact component part number"});
    }
    if (manifest.component.part_number_suffix.has_value()) {
        const auto& suffix = *manifest.component.part_number_suffix;
        if (!ends_with(manifest.component.part_number, suffix)) {
            add(report, IntakeDiagnosticCode::part_number_suffix_mismatch,
                IntakeDiagnosticSeverity::error, "component.part_number_suffix",
                "part_number_suffix is not the exact suffix of part_number");
        }
        if (std::find(
                manifest.source_document.associated_part_numbers.begin(),
                manifest.source_document.associated_part_numbers.end(),
                manifest.component.part_number) ==
            manifest.source_document.associated_part_numbers.end()) {
            add(report, IntakeDiagnosticCode::part_number_suffix_mismatch,
                IntakeDiagnosticSeverity::error, "source_document.associated_part_numbers",
                "source document has no exact applicable part number for the component suffix");
        }
    }

    std::set<component::EvidenceId> evidence_ids;
    for (std::size_t i = 0; i < manifest.evidence.size(); ++i) {
        const auto& entry = manifest.evidence[i];
        const auto path = "evidence[" + std::to_string(i) + "]";
        const auto location_report = component::validate_evidence_location(entry.location);
        report.domain_validation.issues.insert(
            report.domain_validation.issues.end(),
            location_report.issues.begin(),
            location_report.issues.end());
        if (!evidence_ids.insert(entry.location.id).second) {
            report.domain_validation.issues.push_back({
                component::ValidationRuleCode::duplicate_identifier,
                path + ".location.id",
                "duplicate EvidenceId"});
        }
        if (entry.location.source_document_id != manifest.source_document.id) {
            report.domain_validation.issues.push_back({
                component::ValidationRuleCode::unresolved_reference,
                path + ".location.source_document_id",
                "evidence must reference the manifest source document"});
        }
        if (entry.pdf_page_index_was_derived) {
            add(report, IntakeDiagnosticCode::derived_page_index,
                IntakeDiagnosticSeverity::error, path + ".location.pdf_page_index",
                "pdf page index must be explicitly supplied, not derived from printed label");
        }
        if (manifest.declared_pdf_page_count.has_value() &&
            entry.location.pdf_page_index.has_value() &&
            *entry.location.pdf_page_index >= *manifest.declared_pdf_page_count) {
            add(report, IntakeDiagnosticCode::page_index_out_of_range,
                IntakeDiagnosticSeverity::error, path + ".location.pdf_page_index",
                "pdf page index is outside the declared page count");
        }
    }

    std::set<component::ObservationId> observation_ids;
    for (std::size_t i = 0; i < manifest.observations.size(); ++i) {
        const auto& entry = manifest.observations[i];
        const auto& observation = entry.observation;
        const auto path = "observations[" + std::to_string(i) + "]";
        const auto observation_report = component::validate_parameter_observation(observation);
        report.domain_validation.issues.insert(
            report.domain_validation.issues.end(),
            observation_report.issues.begin(),
            observation_report.issues.end());
        if (!observation_ids.insert(observation.id).second) {
            report.domain_validation.issues.push_back({
                component::ValidationRuleCode::duplicate_identifier,
                path + ".observation.id",
                "duplicate ObservationId"});
        }
        if (observation.review_state != component::ReviewState::candidate) {
            add(report, IntakeDiagnosticCode::non_candidate_state_in_intake,
                IntakeDiagnosticSeverity::error, path + ".observation.review_state",
                "intake observations must remain candidates");
        }
        if (!observation.review_history.empty()) {
            add(report, IntakeDiagnosticCode::non_empty_review_history_in_intake,
                IntakeDiagnosticSeverity::error, path + ".observation.review_history",
                "intake observations cannot contain review decisions");
        }
        if (observation.superseded_by.has_value()) {
            add(report, IntakeDiagnosticCode::superseded_observation_in_intake,
                IntakeDiagnosticSeverity::error, path + ".observation.superseded_by",
                "intake observations cannot be superseded");
        }
        for (const auto& reference : observation.evidence_refs) {
            if (evidence_ids.find(reference) == evidence_ids.end()) {
                report.domain_validation.issues.push_back({
                    component::ValidationRuleCode::unresolved_reference,
                    path + ".observation.evidence_refs",
                    "observation references unknown EvidenceId"});
            }
        }
        if (observation.original_value_text.has_value() &&
            is_ambiguous_numeric_text(*observation.original_value_text)) {
            if (observation.parsed_numeric_value.has_value()) {
                add(report, IntakeDiagnosticCode::asserted_ambiguous_numeric_value,
                    IntakeDiagnosticSeverity::error, path + ".observation.parsed_numeric_value",
                    "an ambiguous raw value cannot have an asserted parsed numeric value");
            } else {
                add(report, IntakeDiagnosticCode::ambiguous_numeric_text,
                    IntakeDiagnosticSeverity::warning, path + ".observation.original_value_text",
                    "raw value is preserved but is not safely parseable");
            }
        }
        if (observation.original_value_text.has_value() &&
            observation.parsed_numeric_value.has_value()) {
            const auto raw_value = parse_unambiguous_scalar(*observation.original_value_text);
            if (raw_value.has_value() && *raw_value != *observation.parsed_numeric_value) {
                add(report, IntakeDiagnosticCode::raw_numeric_mismatch,
                    IntakeDiagnosticSeverity::error, path + ".observation.parsed_numeric_value",
                    "parsed numeric value does not exactly match the unambiguous raw scalar");
            }
        }
        if (entry.raw_qualifier_text.has_value() &&
            !qualifier_matches(*entry.raw_qualifier_text, observation.qualifier)) {
            add(report, IntakeDiagnosticCode::qualifier_text_mismatch,
                IntakeDiagnosticSeverity::error, path + ".raw_qualifier_text",
                "raw qualifier conflicts with the structured qualifier");
        }
        if (observation.parsed_numeric_value.has_value() &&
            !std::isfinite(*observation.parsed_numeric_value)) {
            report.domain_validation.issues.push_back({
                component::ValidationRuleCode::non_finite_numeric_value,
                path + ".observation.parsed_numeric_value",
                "parsed numeric value must be finite"});
        }
        for (const auto& evidence : manifest.evidence) {
            if (std::find(
                    observation.evidence_refs.begin(),
                    observation.evidence_refs.end(),
                    evidence.location.id) != observation.evidence_refs.end() &&
                section_suggests_absolute_maximum(evidence) &&
                observation.category != ParameterCategory::absolute_maximum_rating) {
                add(report, IntakeDiagnosticCode::category_section_mismatch,
                    IntakeDiagnosticSeverity::error, path + ".observation.category",
                    "absolute maximum evidence requires absolute_maximum_rating category");
            }
        }
    }
    for (std::size_t left = 0; left < manifest.observations.size(); ++left) {
        const auto& lhs = manifest.observations[left].observation;
        if (!lhs.parsed_numeric_value.has_value()) continue;
        for (std::size_t right = left + 1; right < manifest.observations.size(); ++right) {
            const auto& rhs = manifest.observations[right].observation;
            if (lhs.parameter_name != rhs.parameter_name ||
                lhs.original_unit_symbol != rhs.original_unit_symbol ||
                lhs.category != rhs.category ||
                lhs.operating_conditions != rhs.operating_conditions ||
                !rhs.parsed_numeric_value.has_value()) {
                continue;
            }
            if (lhs.qualifier == rhs.qualifier &&
                *lhs.parsed_numeric_value != *rhs.parsed_numeric_value) {
                add(report, IntakeDiagnosticCode::conflict_candidate,
                    IntakeDiagnosticSeverity::warning,
                    "observations[" + std::to_string(left) + "]",
                    "same parameter slot has competing values; both candidates are retained");
            }
            if (lhs.qualifier == ValueQualifier::minimum &&
                rhs.qualifier == ValueQualifier::maximum &&
                *lhs.parsed_numeric_value > *rhs.parsed_numeric_value) {
                report.domain_validation.issues.push_back({
                    component::ValidationRuleCode::reversed_qualifier_bounds,
                    "observations[" + std::to_string(left) + "]",
                    "minimum value exceeds maximum value for parameter " + lhs.parameter_name});
            }
            if (lhs.qualifier == ValueQualifier::maximum &&
                rhs.qualifier == ValueQualifier::minimum &&
                *lhs.parsed_numeric_value < *rhs.parsed_numeric_value) {
                report.domain_validation.issues.push_back({
                    component::ValidationRuleCode::reversed_qualifier_bounds,
                    "observations[" + std::to_string(left) + "]",
                    "minimum value exceeds maximum value for parameter " + lhs.parameter_name});
            }
        }
    }
    return report;
}

} // namespace datasim::intake
