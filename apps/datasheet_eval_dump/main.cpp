#include "datasim/pdf/table_extraction.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

using datasim::component::ActorKind;
using datasim::component::ComponentFamily;
using datasim::component::ExtractionMethod;
using datasim::component::ParameterCategory;
using datasim::component::ReviewState;
using datasim::component::ValidationRuleCode;
using datasim::component::ValueQualifier;
using datasim::intake::IntakeDiagnosticCode;
using datasim::intake::IntakeDiagnosticSeverity;
using datasim::pdf::HeaderConditionBinding;
using datasim::pdf::HeaderUnitBinding;
using datasim::pdf::HorizontalSpan;

std::vector<std::string> get_utf8_args(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == nullptr) {
        throw std::runtime_error("CommandLineToArgvW failed");
    }
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(wargc));
    for (int i = 0; i < wargc; ++i) {
        const std::wstring_view warg(wargv[i]);
        if (warg.empty()) {
            result.emplace_back();
            continue;
        }
        const int size = WideCharToMultiByte(
            CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) {
            LocalFree(wargv);
            throw std::runtime_error("WideCharToMultiByte failed");
        }
        std::string utf8(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), utf8.data(), size, nullptr, nullptr);
        result.push_back(std::move(utf8));
    }
    LocalFree(wargv);
    return result;
#else
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        result.emplace_back(argv[i]);
    }
    return result;
#endif
}

ValueQualifier parse_qualifier(const std::string& text) {
    if (text == "minimum" || text == "min") return ValueQualifier::minimum;
    if (text == "typical" || text == "typ") return ValueQualifier::typical;
    if (text == "maximum" || text == "max") return ValueQualifier::maximum;
    throw std::invalid_argument("invalid header unit binding qualifier: " + text);
}

std::vector<std::string> split_pipe_fields(const std::string& spec, std::size_t count) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i + 1U < count; ++i) {
        const auto bar = spec.find('|', start);
        if (bar == std::string::npos) {
            throw std::invalid_argument("invalid pipe-delimited binding specification: " + spec);
        }
        parts.push_back(spec.substr(start, bar - start));
        start = bar + 1U;
    }
    parts.push_back(spec.substr(start));
    return parts;
}

std::string json_escape(std::string_view input) {
    std::ostringstream out;
    for (unsigned char ch : input) {
        switch (ch) {
        case '"':
            out << "\\\"";
            break;
        case '\\':
            out << "\\\\";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        default:
            if (ch < 0x20U) {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<unsigned int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

void write_json_string_or_null(std::ostream& out, const std::optional<std::string>& value) {
    if (value.has_value()) {
        out << "\"" << json_escape(*value) << "\"";
    } else {
        out << "null";
    }
}

void write_json_double_or_null(std::ostream& out, const std::optional<double>& value) {
    if (value.has_value() && std::isfinite(*value)) {
        std::ostringstream num;
        num << std::setprecision(15) << *value;
        out << num.str();
    } else {
        out << "null";
    }
}

std::vector<double> parse_csv_numbers(const std::string& text, std::size_t expected_count) {
    std::vector<double> values;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        std::size_t consumed = 0;
        const double value = std::stod(item, &consumed);
        if (consumed != item.size()) {
            throw std::invalid_argument("invalid number in list: " + text);
        }
        values.push_back(value);
    }
    if (values.size() != expected_count) {
        throw std::invalid_argument("unexpected number count in: " + text);
    }
    return values;
}

HorizontalSpan parse_span(const std::string& text) {
    const auto values = parse_csv_numbers(text, 2);
    return HorizontalSpan{values[0], values[1]};
}

bool parse_bool(const std::string& text) {
    if (text == "true" || text == "1" || text == "yes") {
        return true;
    }
    if (text == "false" || text == "0" || text == "no") {
        return false;
    }
    throw std::invalid_argument("invalid boolean value: " + text);
}

ComponentFamily parse_family(const std::string& text) {
    if (text == "resistor") {
        return ComponentFamily::resistor;
    }
    if (text == "capacitor") {
        return ComponentFamily::capacitor;
    }
    if (text == "inductor") {
        return ComponentFamily::inductor;
    }
    if (text == "other") {
        return ComponentFamily::other;
    }
    throw std::invalid_argument("invalid component family: " + text);
}

ParameterCategory parse_category(const std::string& text) {
    if (text == "absolute_maximum_rating") {
        return ParameterCategory::absolute_maximum_rating;
    }
    if (text == "recommended_operating_condition") {
        return ParameterCategory::recommended_operating_condition;
    }
    if (text == "electrical_characteristic") {
        return ParameterCategory::electrical_characteristic;
    }
    if (text == "thermal_characteristic") {
        return ParameterCategory::thermal_characteristic;
    }
    if (text == "other_unspecified") {
        return ParameterCategory::other_unspecified;
    }
    throw std::invalid_argument("invalid parameter category: " + text);
}

std::string_view qualifier_to_string(ValueQualifier q) noexcept {
    switch (q) {
    case ValueQualifier::minimum:
        return "minimum";
    case ValueQualifier::typical:
        return "typical";
    case ValueQualifier::maximum:
        return "maximum";
    case ValueQualifier::unspecified:
        return "unspecified";
    }
    return "unspecified";
}

std::string_view category_to_string(ParameterCategory c) noexcept {
    switch (c) {
    case ParameterCategory::absolute_maximum_rating:
        return "absolute_maximum_rating";
    case ParameterCategory::recommended_operating_condition:
        return "recommended_operating_condition";
    case ParameterCategory::electrical_characteristic:
        return "electrical_characteristic";
    case ParameterCategory::thermal_characteristic:
        return "thermal_characteristic";
    case ParameterCategory::other_unspecified:
        return "other_unspecified";
    }
    return "other_unspecified";
}

std::string_view extraction_method_to_string(ExtractionMethod m) noexcept {
    switch (m) {
    case ExtractionMethod::manual_entry:
        return "manual_entry";
    case ExtractionMethod::synthetic_fixture:
        return "synthetic_fixture";
    case ExtractionMethod::deterministic_table_parser:
        return "deterministic_table_parser";
    case ExtractionMethod::deterministic_text_parser:
        return "deterministic_text_parser";
    case ExtractionMethod::curve_digitization:
        return "curve_digitization";
    case ExtractionMethod::ocr:
        return "ocr";
    case ExtractionMethod::llm_assisted:
        return "llm_assisted";
    }
    return "deterministic_table_parser";
}

std::string_view review_state_to_string(ReviewState s) noexcept {
    switch (s) {
    case ReviewState::candidate:
        return "candidate";
    case ReviewState::needs_review:
        return "needs_review";
    case ReviewState::approved:
        return "approved";
    case ReviewState::rejected:
        return "rejected";
    case ReviewState::superseded:
        return "superseded";
    }
    return "candidate";
}

std::string_view intake_severity_to_string(IntakeDiagnosticSeverity s) noexcept {
    switch (s) {
    case IntakeDiagnosticSeverity::warning:
        return "warning";
    case IntakeDiagnosticSeverity::review_required:
        return "review_required";
    case IntakeDiagnosticSeverity::error:
        return "error";
    }
    return "error";
}

std::string_view intake_code_to_string(IntakeDiagnosticCode code) noexcept {
    switch (code) {
    case IntakeDiagnosticCode::invalid_schema_version:
        return "invalid_schema_version";
    case IntakeDiagnosticCode::invalid_origin:
        return "invalid_origin";
    case IntakeDiagnosticCode::invalid_curator:
        return "invalid_curator";
    case IntakeDiagnosticCode::manufacturer_publisher_mismatch:
        return "manufacturer_publisher_mismatch";
    case IntakeDiagnosticCode::part_number_suffix_mismatch:
        return "part_number_suffix_mismatch";
    case IntakeDiagnosticCode::page_index_out_of_range:
        return "page_index_out_of_range";
    case IntakeDiagnosticCode::derived_page_index:
        return "derived_page_index";
    case IntakeDiagnosticCode::inconsistent_synthetic_markers:
        return "inconsistent_synthetic_markers";
    case IntakeDiagnosticCode::synthetic_masquerading_as_manufacturer:
        return "synthetic_masquerading_as_manufacturer";
    case IntakeDiagnosticCode::invalid_intake_entry:
        return "invalid_intake_entry";
    case IntakeDiagnosticCode::ambiguous_numeric_text:
        return "ambiguous_numeric_text";
    case IntakeDiagnosticCode::asserted_ambiguous_numeric_value:
        return "asserted_ambiguous_numeric_value";
    case IntakeDiagnosticCode::raw_numeric_mismatch:
        return "raw_numeric_mismatch";
    case IntakeDiagnosticCode::unsupported_unit_conversion:
        return "unsupported_unit_conversion";
    case IntakeDiagnosticCode::qualifier_text_mismatch:
        return "qualifier_text_mismatch";
    case IntakeDiagnosticCode::category_section_mismatch:
        return "category_section_mismatch";
    case IntakeDiagnosticCode::non_candidate_state_in_intake:
        return "non_candidate_state_in_intake";
    case IntakeDiagnosticCode::non_empty_review_history_in_intake:
        return "non_empty_review_history_in_intake";
    case IntakeDiagnosticCode::superseded_observation_in_intake:
        return "superseded_observation_in_intake";
    case IntakeDiagnosticCode::conflict_candidate:
        return "conflict_candidate";
    case IntakeDiagnosticCode::unsupported_embedded_content:
        return "unsupported_embedded_content";
    }
    return "invalid_intake_entry";
}

std::string_view domain_rule_to_string(ValidationRuleCode code) noexcept {
    switch (code) {
    case ValidationRuleCode::empty_required_field:
        return "empty_required_field";
    case ValidationRuleCode::invalid_identifier:
        return "invalid_identifier";
    case ValidationRuleCode::duplicate_identifier:
        return "duplicate_identifier";
    case ValidationRuleCode::unresolved_reference:
        return "unresolved_reference";
    case ValidationRuleCode::non_finite_numeric_value:
        return "non_finite_numeric_value";
    case ValidationRuleCode::missing_numeric_source_text:
        return "missing_numeric_source_text";
    case ValidationRuleCode::missing_required_unit:
        return "missing_required_unit";
    case ValidationRuleCode::contradictory_unitless_flag:
        return "contradictory_unitless_flag";
    case ValidationRuleCode::invalid_date_or_timestamp:
        return "invalid_date_or_timestamp";
    case ValidationRuleCode::invalid_sha256_digest:
        return "invalid_sha256_digest";
    case ValidationRuleCode::empty_evidence_locator:
        return "empty_evidence_locator";
    case ValidationRuleCode::missing_evidence_reference:
        return "missing_evidence_reference";
    case ValidationRuleCode::invalid_review_transition:
        return "invalid_review_transition";
    case ValidationRuleCode::invalid_review_history:
        return "invalid_review_history";
    case ValidationRuleCode::unauthorized_reviewer_kind:
        return "unauthorized_reviewer_kind";
    case ValidationRuleCode::self_approval_prohibited:
        return "self_approval_prohibited";
    case ValidationRuleCode::missing_superseding_reference:
        return "missing_superseding_reference";
    case ValidationRuleCode::unexpected_superseding_reference:
        return "unexpected_superseding_reference";
    case ValidationRuleCode::invalid_superseding_target:
        return "invalid_superseding_target";
    case ValidationRuleCode::missing_value_cannot_be_approved:
        return "missing_value_cannot_be_approved";
    case ValidationRuleCode::unapproved_observation_in_knowledge:
        return "unapproved_observation_in_knowledge";
    case ValidationRuleCode::category_mismatch:
        return "category_mismatch";
    case ValidationRuleCode::qualifier_mismatch:
        return "qualifier_mismatch";
    case ValidationRuleCode::original_notation_mismatch:
        return "original_notation_mismatch";
    case ValidationRuleCode::reversed_qualifier_bounds:
        return "reversed_qualifier_bounds";
    case ValidationRuleCode::undeclared_parameter_conflict:
        return "undeclared_parameter_conflict";
    case ValidationRuleCode::invalid_conflict_record:
        return "invalid_conflict_record";
    case ValidationRuleCode::conflicted_observation_promoted:
        return "conflicted_observation_promoted";
    case ValidationRuleCode::invalid_version_history:
        return "invalid_version_history";
    case ValidationRuleCode::synthetic_provenance_mismatch:
        return "synthetic_provenance_mismatch";
    case ValidationRuleCode::absolute_maximum_misused_as_model_parameter:
        return "absolute_maximum_misused_as_model_parameter";
    case ValidationRuleCode::invalid_terminal_mapping:
        return "invalid_terminal_mapping";
    case ValidationRuleCode::invalid_model_version:
        return "invalid_model_version";
    case ValidationRuleCode::missing_model_source_or_license:
        return "missing_model_source_or_license";
    case ValidationRuleCode::missing_artifact_reference:
        return "missing_artifact_reference";
    case ValidationRuleCode::invalid_parameter_mapping:
        return "invalid_parameter_mapping";
    case ValidationRuleCode::invalid_validity_envelope:
        return "invalid_validity_envelope";
    case ValidationRuleCode::empty_supported_analyses:
        return "empty_supported_analyses";
    case ValidationRuleCode::empty_model_limitations:
        return "empty_model_limitations";
    case ValidationRuleCode::invalid_model_validation_record:
        return "invalid_model_validation_record";
    case ValidationRuleCode::incompatible_parameter_units:
        return "incompatible_parameter_units";
    case ValidationRuleCode::incompatible_source_document_part_number:
        return "incompatible_source_document_part_number";
    }
    return "empty_required_field";
}

void write_extraction_result_json(std::ostream& out, const datasim::pdf::ExtractionResult& result) {
    out << "{\n";
    out << "  \"valid_report\": " << (result.report.valid() ? "true" : "false") << ",\n";
    out << "  \"has_manifest\": " << (result.manifest.has_value() ? "true" : "false") << ",\n";
    out << "  \"has_candidate_bundle\": " << (result.candidate_bundle.has_value() ? "true" : "false") << ",\n";
    out << "  \"declared_pdf_page_count\": ";
    if (result.manifest.has_value() && result.manifest->declared_pdf_page_count.has_value()) {
        out << *result.manifest->declared_pdf_page_count;
    } else {
        out << "null";
    }
    out << ",\n";

    out << "  \"extraction_diagnostics\": [\n";
    for (std::size_t i = 0; i < result.diagnostics.size(); ++i) {
        out << "    \"" << json_escape(result.diagnostics[i]) << "\""
            << (i + 1 < result.diagnostics.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"intake_diagnostics\": [\n";
    for (std::size_t i = 0; i < result.report.diagnostics.size(); ++i) {
        const auto& diag = result.report.diagnostics[i];
        out << "    {\n";
        out << "      \"code\": \"" << intake_code_to_string(diag.code) << "\",\n";
        out << "      \"severity\": \"" << intake_severity_to_string(diag.severity) << "\",\n";
        out << "      \"entity_path\": \"" << json_escape(diag.entity_path) << "\",\n";
        out << "      \"message\": \"" << json_escape(diag.message) << "\"\n";
        out << "    }" << (i + 1 < result.report.diagnostics.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"domain_validation_issues\": [\n";
    for (std::size_t i = 0; i < result.report.domain_validation.issues.size(); ++i) {
        const auto& issue = result.report.domain_validation.issues[i];
        out << "    {\n";
        out << "      \"code\": \"" << domain_rule_to_string(issue.code) << "\",\n";
        out << "      \"entity_path\": \"" << json_escape(issue.entity_path) << "\",\n";
        out << "      \"message\": \"" << json_escape(issue.message) << "\"\n";
        out << "    }" << (i + 1 < result.report.domain_validation.issues.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"evidence\": [\n";
    if (result.manifest.has_value()) {
        const auto& evidence_list = result.manifest->evidence;
        for (std::size_t i = 0; i < evidence_list.size(); ++i) {
            const auto& entry = evidence_list[i];
            const auto& loc = entry.location;
            out << "    {\n";
            out << "      \"id\": \"" << json_escape(loc.id.value) << "\",\n";
            out << "      \"source_document_id\": \"" << json_escape(loc.source_document_id.value) << "\",\n";
            out << "      \"pdf_page_index\": ";
            if (loc.pdf_page_index.has_value()) {
                out << *loc.pdf_page_index;
            } else {
                out << "null";
            }
            out << ",\n";
            out << "      \"printed_page_label\": ";
            write_json_string_or_null(out, loc.printed_page_label);
            out << ",\n";
            out << "      \"section\": ";
            write_json_string_or_null(out, loc.section);
            out << ",\n";
            out << "      \"table_or_figure_id\": ";
            write_json_string_or_null(out, loc.table_or_figure_id);
            out << ",\n";
            out << "      \"cell_or_region\": ";
            write_json_string_or_null(out, loc.cell_or_region);
            out << ",\n";
            out << "      \"footnote_refs\": [";
            for (std::size_t f = 0; f < loc.footnote_refs.size(); ++f) {
                out << (f > 0 ? ", " : "") << "\"" << json_escape(loc.footnote_refs[f]) << "\"";
            }
            out << "],\n";
            out << "      \"row_label\": ";
            write_json_string_or_null(out, entry.row_label);
            out << ",\n";
            out << "      \"column_label\": ";
            write_json_string_or_null(out, entry.column_label);
            out << ",\n";
            out << "      \"pdf_page_index_was_derived\": "
                << (entry.pdf_page_index_was_derived ? "true" : "false") << "\n";
            out << "    }" << (i + 1 < evidence_list.size() ? "," : "") << "\n";
        }
    }
    out << "  ],\n";

    out << "  \"observations\": [\n";
    if (result.manifest.has_value()) {
        const auto& obs_list = result.manifest->observations;
        for (std::size_t i = 0; i < obs_list.size(); ++i) {
            const auto& entry = obs_list[i];
            const auto& obs = entry.observation;
            out << "    {\n";
            out << "      \"id\": \"" << json_escape(obs.id.value) << "\",\n";
            out << "      \"component_id\": \"" << json_escape(obs.component_id.value) << "\",\n";
            out << "      \"parameter_name\": \"" << json_escape(obs.parameter_name) << "\",\n";
            out << "      \"original_value_text\": ";
            write_json_string_or_null(out, obs.original_value_text);
            out << ",\n";
            out << "      \"original_unit_symbol\": ";
            write_json_string_or_null(out, obs.original_unit_symbol);
            out << ",\n";
            out << "      \"unitless\": " << (obs.unitless ? "true" : "false") << ",\n";
            out << "      \"parsed_numeric_value\": ";
            write_json_double_or_null(out, obs.parsed_numeric_value);
            out << ",\n";
            out << "      \"qualifier\": \"" << qualifier_to_string(obs.qualifier) << "\",\n";
            out << "      \"category\": \"" << category_to_string(obs.category) << "\",\n";
            out << "      \"operating_conditions\": [\n";
            for (std::size_t c = 0; c < obs.operating_conditions.size(); ++c) {
                const auto& cond = obs.operating_conditions[c];
                out << "        {\n";
                out << "          \"parameter_name\": \"" << json_escape(cond.parameter_name) << "\",\n";
                out << "          \"original_value_text\": \"" << json_escape(cond.original_value_text) << "\",\n";
                out << "          \"unit_symbol\": \"" << json_escape(cond.unit_symbol) << "\",\n";
                out << "          \"numeric_value\": ";
                write_json_double_or_null(out, cond.numeric_value);
                out << ",\n";
                out << "          \"notes\": ";
                write_json_string_or_null(out, cond.notes);
                out << "\n";
                out << "        }" << (c + 1 < obs.operating_conditions.size() ? "," : "") << "\n";
            }
            out << "      ],\n";
            out << "      \"evidence_refs\": [";
            for (std::size_t e = 0; e < obs.evidence_refs.size(); ++e) {
                out << (e > 0 ? ", " : "") << "\"" << json_escape(obs.evidence_refs[e].value) << "\"";
            }
            out << "],\n";
            out << "      \"extraction_method\": \"" << extraction_method_to_string(obs.extraction_method) << "\",\n";
            out << "      \"extraction_tool\": ";
            write_json_string_or_null(out, obs.extraction_tool);
            out << ",\n";
            out << "      \"extraction_tool_version\": ";
            write_json_string_or_null(out, obs.extraction_tool_version);
            out << ",\n";
            out << "      \"extracted_at\": ";
            write_json_string_or_null(out, obs.extracted_at);
            out << ",\n";
            out << "      \"uncertainty_note\": ";
            write_json_string_or_null(out, obs.uncertainty_note);
            out << ",\n";
            out << "      \"review_state\": \"" << review_state_to_string(obs.review_state) << "\",\n";
            out << "      \"review_history_count\": " << obs.review_history.size() << ",\n";
            out << "      \"superseded_by\": ";
            if (obs.superseded_by.has_value()) {
                out << "\"" << json_escape(obs.superseded_by->value) << "\"";
            } else {
                out << "null";
            }
            out << ",\n";
            out << "      \"raw_symbol\": ";
            write_json_string_or_null(out, entry.raw_symbol);
            out << ",\n";
            out << "      \"raw_qualifier_text\": ";
            write_json_string_or_null(out, entry.raw_qualifier_text);
            out << ",\n";
            out << "      \"raw_conditions_text\": ";
            write_json_string_or_null(out, entry.raw_conditions_text);
            out << ",\n";
            out << "      \"ambiguity_notes\": ";
            write_json_string_or_null(out, entry.ambiguity_notes);
            out << "\n";
            out << "    }" << (i + 1 < obs_list.size() ? "," : "") << "\n";
        }
    }
    out << "  ]\n";
    out << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        datasim::pdf::ExtractionRequest request;
        request.curator = {
            "CURATOR-EVAL-1",
            ActorKind::automatic_parser,
            "Datasim Evaluation Harness"};
        request.component.family = ComponentFamily::other;
        request.component.is_synthetic = false;
        request.source_document.is_synthetic = false;
        request.source_document.usage_rights = {
            std::nullopt,
            false,
            "Local evaluation only; redistribution not permitted."};
        request.table.category = ParameterCategory::electrical_characteristic;

        bool has_region = false;
        bool has_param = false;
        bool has_unit = false;
        bool has_min = false;
        bool has_typ = false;
        bool has_max = false;
        bool has_page = false;
        std::optional<std::string> explicit_publisher;
        std::vector<std::string> explicit_doc_part_numbers;
        std::optional<std::filesystem::path> output_path;

        const auto args = get_utf8_args(argc, argv);
        for (std::size_t i = 1; i < args.size(); ++i) {
            const std::string& arg = args[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= args.size()) {
                    throw std::invalid_argument("missing value for " + arg);
                }
                return args[++i];
            };

            if (arg == "--pdf") {
                request.pdf_path = next();
            } else if (arg == "--pdfinfo") {
                request.pdfinfo_path = next();
            } else if (arg == "--pdftotext") {
                request.pdftotext_path = next();
            } else if (arg == "--page-index") {
                const auto value = next();
                std::size_t consumed = 0;
                request.pdf_page_index = static_cast<std::size_t>(std::stoul(value, &consumed));
                if (consumed != value.size()) {
                    throw std::invalid_argument("invalid page index: " + value);
                }
                has_page = true;
            } else if (arg == "--printed-page") {
                request.printed_page_label = next();
            } else if (arg == "--curator-id") {
                request.curator.id = next();
            } else if (arg == "--curator-name") {
                request.curator.display_name = next();
            } else if (arg == "--component-id") {
                request.component.id = datasim::component::ComponentId{next()};
            } else if (arg == "--manufacturer") {
                request.component.manufacturer = next();
            } else if (arg == "--part-number") {
                request.component.part_number = next();
            } else if (arg == "--part-suffix") {
                request.component.part_number_suffix = next();
            } else if (arg == "--package-id") {
                request.component.package_id = next();
            } else if (arg == "--family") {
                request.component.family = parse_family(next());
            } else if (arg == "--subtype") {
                request.component.subtype = next();
            } else if (arg == "--synthetic") {
                const bool syn = parse_bool(next());
                request.component.is_synthetic = syn;
                request.source_document.is_synthetic = syn;
            } else if (arg == "--document-id") {
                request.source_document.id = datasim::component::SourceDocumentId{next()};
            } else if (arg == "--publisher") {
                explicit_publisher = next();
            } else if (arg == "--title") {
                request.source_document.title = next();
            } else if (arg == "--doc-part-number") {
                explicit_doc_part_numbers.push_back(next());
            } else if (arg == "--revision") {
                request.source_document.revision = next();
            } else if (arg == "--original-url") {
                request.source_document.original_url = next();
            } else if (arg == "--retrieval-date") {
                request.source_document.retrieval_date = next();
            } else if (arg == "--sha256") {
                request.source_document.content_sha256 = next();
            } else if (arg == "--license-id") {
                request.source_document.usage_rights.license_identifier = next();
            } else if (arg == "--redistribution-permitted") {
                request.source_document.usage_rights.redistribution_permitted = parse_bool(next());
            } else if (arg == "--restriction-notes") {
                request.source_document.usage_rights.restriction_notes = next();
            } else if (arg == "--table-id") {
                request.table.table_id = next();
            } else if (arg == "--region") {
                const auto coords = parse_csv_numbers(next(), 4);
                request.table.region_x_min = coords[0];
                request.table.region_y_min = coords[1];
                request.table.region_x_max = coords[2];
                request.table.region_y_max = coords[3];
                has_region = true;
            } else if (arg == "--row-y-tolerance") {
                const auto vals = parse_csv_numbers(next(), 1);
                request.table.row_y_tolerance = vals[0];
            } else if (arg == "--header") {
                const auto coords = parse_csv_numbers(next(), 2);
                request.table.header_y_min = coords[0];
                request.table.header_y_max = coords[1];
            } else if (arg == "--parameter-column") {
                request.table.parameter_column = parse_span(next());
                has_param = true;
            } else if (arg == "--unit-column") {
                request.table.unit_column = parse_span(next());
                has_unit = true;
            } else if (arg == "--min-column") {
                request.table.minimum_column = parse_span(next());
                has_min = true;
            } else if (arg == "--typ-column") {
                request.table.typical_column = parse_span(next());
                has_typ = true;
            } else if (arg == "--max-column") {
                request.table.maximum_column = parse_span(next());
                has_max = true;
            } else if (arg == "--condition-column") {
                request.table.condition_column = parse_span(next());
            } else if (arg == "--category") {
                request.table.category = parse_category(next());
            } else if (arg == "--header-unit-binding") {
                const auto parts = split_pipe_fields(next(), 4);
                const auto box = parse_csv_numbers(parts[1], 4);
                request.table.header_unit_bindings.push_back(HeaderUnitBinding{
                    parse_qualifier(parts[0]),
                    HorizontalSpan{box[0], box[2]},
                    box[1],
                    box[3],
                    parts[3],
                    parts[2]});
            } else if (arg == "--header-condition-binding") {
                const auto parts = split_pipe_fields(next(), 4);
                const auto box = parse_csv_numbers(parts[0], 4);
                request.table.header_condition_binding = HeaderConditionBinding{
                    HorizontalSpan{box[0], box[2]},
                    box[1],
                    box[3],
                    parts[3],
                    parts[1],
                    parts[2]};
            } else if (arg == "--output") {
                output_path = next();
            } else {
                throw std::invalid_argument("unknown argument: " + arg);
            }
        }

        if (!has_page || !has_region || !has_param || !has_unit || !has_min || !has_typ || !has_max ||
            request.pdf_path.empty() || request.pdfinfo_path.empty() || request.pdftotext_path.empty() ||
            request.component.id.value.empty() || request.component.manufacturer.empty() ||
            request.component.part_number.empty() || request.source_document.id.value.empty() ||
            request.source_document.title.empty() || request.source_document.revision.empty() ||
            !request.source_document.content_sha256.has_value() || request.table.table_id.empty()) {
            throw std::invalid_argument("missing required extraction option");
        }

        request.source_document.publisher =
            explicit_publisher.has_value() ? *explicit_publisher : request.component.manufacturer;
        if (!explicit_doc_part_numbers.empty()) {
            request.source_document.associated_part_numbers = explicit_doc_part_numbers;
        } else {
            request.source_document.associated_part_numbers = {request.component.part_number};
        }

        const auto result = datasim::pdf::extract_electrical_table(request);

        if (output_path.has_value()) {
            std::ofstream file(*output_path, std::ios::binary | std::ios::trunc);
            if (!file) {
                throw std::runtime_error("failed to open output file: " + output_path->string());
            }
            write_extraction_result_json(file, result);
        } else {
            write_extraction_result_json(std::cout, result);
        }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "datasheet_eval_dump error: " << ex.what() << "\n";
        return 2;
    }
}
