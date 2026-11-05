#pragma once

#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/component/observation/review.hpp"
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/evidence/source_document.hpp"
#include "datasim/intake/intake.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace datasim::pdf {

struct HorizontalSpan {
    double min = 0.0;
    double max = 0.0;
};

struct HeaderUnitBinding {
    component::ValueQualifier qualifier = component::ValueQualifier::typical;
    HorizontalSpan header_x_span;
    double header_y_min = 0.0;
    double header_y_max = 0.0;
    std::string expected_header_text;
    std::string unit_symbol;
};

struct HeaderConditionBinding {
    HorizontalSpan header_x_span;
    double header_y_min = 0.0;
    double header_y_max = 0.0;
    std::string expected_header_text;
    std::string parameter_name;
    std::string unit_symbol;
};

struct InlineValueUnitBinding {
    component::ValueQualifier qualifier = component::ValueQualifier::typical;
    std::vector<std::string> allowed_unit_symbols;
};

struct ValueCellProvenance {
    component::ObservationId observation_id;
    component::EvidenceId value_evidence_id;
    std::size_t row_index = 0;
    component::ValueQualifier qualifier = component::ValueQualifier::unspecified;
    std::string raw_cell_text;
    std::string raw_value_token;
    std::optional<std::string> raw_inline_unit_token;

    auto operator<=>(const ValueCellProvenance&) const = default;
};

struct ElectricalTableLayout {
    std::string table_id;
    double region_x_min = 0.0;
    double region_y_min = 0.0;
    double region_x_max = 0.0;
    double region_y_max = 0.0;
    double row_y_tolerance = 2.0;
    double header_y_min = 0.0;
    double header_y_max = 0.0;
    HorizontalSpan parameter_column;
    HorizontalSpan unit_column;
    HorizontalSpan minimum_column;
    HorizontalSpan typical_column;
    HorizontalSpan maximum_column;
    std::optional<HorizontalSpan> condition_column;
    std::optional<component::ParameterCategory> category;
    std::vector<HeaderUnitBinding> header_unit_bindings;
    std::optional<HeaderConditionBinding> header_condition_binding;
    std::vector<InlineValueUnitBinding> inline_value_unit_bindings;
};

struct ExtractionRequest {
    std::filesystem::path pdf_path;
    std::filesystem::path pdfinfo_path;
    std::filesystem::path pdftotext_path;
    std::size_t pdf_page_index = 0;
    std::optional<std::string> printed_page_label;
    component::ReviewerIdentity curator;
    component::ComponentIdentity component;
    component::SourceDocument source_document;
    ElectricalTableLayout table;
};

struct ExtractionResult {
    intake::IntakeReport report;
    std::optional<intake::IntakeManifest> manifest;
    std::optional<component::ComponentDomainBundle> candidate_bundle;
    std::vector<std::string> diagnostics;
    std::vector<ValueCellProvenance> value_cell_provenance;
};

// Extracts only explicitly selected horizontal table bands from selectable PDF text.
// Unsupported or ambiguous geometry yields diagnostics and no bundle.
[[nodiscard]] ExtractionResult extract_electrical_table(const ExtractionRequest& request);

} // namespace datasim::pdf
