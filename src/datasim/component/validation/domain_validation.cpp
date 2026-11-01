#include "datasim/component/validation/domain_validation.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace datasim::component {

namespace {

bool is_ascii_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

bool is_ascii_hex(char c) noexcept {
    return is_ascii_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool is_ascii_whitespace(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

bool has_disallowed_control_chars(std::string_view text, bool allow_newlines) noexcept {
    for (unsigned char uc : text) {
        if (uc == 0x7FU) {
            return true;
        }
        if (uc < 0x20U) {
            if (allow_newlines && (uc == '\n' || uc == '\t')) {
                continue;
            }
            return true;
        }
    }
    return false;
}

// Single-line required text: non-empty, contains at least one non-space character,
// and contains no control characters (including CR/LF/TAB).
bool is_non_blank_single_line(std::string_view text) noexcept {
    if (text.empty() || has_disallowed_control_chars(text, false)) {
        return false;
    }
    for (char c : text) {
        if (!is_ascii_whitespace(c)) {
            return true;
        }
    }
    return false;
}

// Multi-line prose text (rationales, summaries, notes): non-blank and free of raw control bytes.
bool is_non_blank_prose(std::string_view text) noexcept {
    if (text.empty() || has_disallowed_control_chars(text, true)) {
        return false;
    }
    for (char c : text) {
        if (!is_ascii_whitespace(c)) {
            return true;
        }
    }
    return false;
}

bool parse_fixed_uint(std::string_view text, std::size_t pos, std::size_t len, unsigned& out) noexcept {
    if (pos + len > text.size()) {
        return false;
    }
    unsigned value = 0;
    for (std::size_t i = 0; i < len; ++i) {
        const char c = text[pos + i];
        if (!is_ascii_digit(c)) {
            return false;
        }
        value = value * 10U + static_cast<unsigned>(c - '0');
    }
    out = value;
    return true;
}

bool is_leap_year(unsigned year) noexcept {
    return (year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U);
}

bool is_valid_calendar_date(std::string_view date_part) noexcept {
    if (date_part.size() != 10U || date_part[4] != '-' || date_part[7] != '-') {
        return false;
    }
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    if (!parse_fixed_uint(date_part, 0, 4, year) ||
        !parse_fixed_uint(date_part, 5, 2, month) ||
        !parse_fixed_uint(date_part, 8, 2, day)) {
        return false;
    }
    if (year < 1900U || month < 1U || month > 12U || day < 1U) {
        return false;
    }
    const unsigned days_in_month[12] = {
        31U,
        is_leap_year(year) ? 29U : 28U,
        31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
    };
    return day <= days_in_month[month - 1U];
}

void add_issue(
    ValidationReport& report,
    ValidationRuleCode code,
    std::string path,
    std::string message) {
    report.issues.push_back(ValidationIssue{code, std::move(path), std::move(message)});
}

void append_report(ValidationReport& dst, ValidationReport src) {
    for (auto& issue : src.issues) {
        dst.issues.push_back(std::move(issue));
    }
}

void check_id_token(
    ValidationReport& report,
    std::string_view token,
    std::string_view path,
    std::string_view label) {
    if (token.empty()) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            std::string(path),
            std::string(label) + " must not be empty");
    } else if (!is_valid_identifier_token(token)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_identifier,
            std::string(path),
            std::string(label) + " contains whitespace or control characters");
    }
}

using ConditionKey = std::vector<std::tuple<std::string, std::string, std::string, std::string>>;

ConditionKey canonical_condition_key(const std::vector<OperatingCondition>& conditions) {
    ConditionKey key;
    key.reserve(conditions.size());
    for (const auto& c : conditions) {
        key.emplace_back(
            c.parameter_name,
            c.original_value_text,
            c.unit_symbol,
            c.notes.value_or(""));
    }
    std::sort(key.begin(), key.end());
    return key;
}

bool requires_extraction_tool(ExtractionMethod method) noexcept {
    switch (method) {
        case ExtractionMethod::manual_entry:
        case ExtractionMethod::synthetic_fixture:
            return false;
        case ExtractionMethod::deterministic_table_parser:
        case ExtractionMethod::deterministic_text_parser:
        case ExtractionMethod::curve_digitization:
        case ExtractionMethod::ocr:
        case ExtractionMethod::llm_assisted:
            return true;
    }
    return true;
}

bool requires_external_artifact_uri(ModelFormat format) noexcept {
    switch (format) {
        case ModelFormat::analytical_equation:
            return false;
        case ModelFormat::spice_model_card:
        case ModelFormat::spice_subcircuit:
        case ModelFormat::lookup_table:
        case ModelFormat::vendor_specific_artifact:
            return true;
    }
    return true;
}

} // namespace

bool is_valid_identifier_token(std::string_view token) noexcept {
    if (token.empty() || has_disallowed_control_chars(token, false)) {
        return false;
    }
    for (char c : token) {
        if (is_ascii_whitespace(c)) {
            return false;
        }
    }
    return true;
}

bool is_valid_version_token(std::string_view token) noexcept {
    return is_valid_identifier_token(token);
}

bool is_valid_sha256_hex(std::string_view digest) noexcept {
    if (digest.size() != 64U) {
        return false;
    }
    for (char c : digest) {
        if (!is_ascii_hex(c)) {
            return false;
        }
    }
    return true;
}

bool is_valid_iso8601_date_or_timestamp(std::string_view text) noexcept {
    if (text.size() == 10U) {
        return is_valid_calendar_date(text);
    }
    if (text.size() < 20U || !is_valid_calendar_date(text.substr(0, 10)) || text[10] != 'T') {
        return false;
    }
    if (text[13] != ':' || text[16] != ':') {
        return false;
    }
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    if (!parse_fixed_uint(text, 11, 2, hour) ||
        !parse_fixed_uint(text, 14, 2, minute) ||
        !parse_fixed_uint(text, 17, 2, second)) {
        return false;
    }
    if (hour > 23U || minute > 59U || second > 60U) {
        return false;
    }
    std::size_t idx = 19U;
    if (idx < text.size() && text[idx] == '.') {
        ++idx;
        const std::size_t frac_start = idx;
        while (idx < text.size() && is_ascii_digit(text[idx])) {
            ++idx;
        }
        if (idx == frac_start) {
            return false;
        }
    }
    if (idx >= text.size()) {
        return false;
    }
    if (text[idx] == 'Z') {
        return idx + 1U == text.size();
    }
    if ((text[idx] == '+' || text[idx] == '-') && idx + 6U == text.size() && text[idx + 3U] == ':') {
        unsigned tz_h = 0;
        unsigned tz_m = 0;
        if (!parse_fixed_uint(text, idx + 1U, 2, tz_h) ||
            !parse_fixed_uint(text, idx + 4U, 2, tz_m)) {
            return false;
        }
        return tz_h <= 23U && tz_m <= 59U;
    }
    return false;
}


enum class PhysicalDimension {
    unknown,
    unitless,
    resistance,
    capacitance,
    inductance,
    voltage,
    current,
    power,
    frequency,
    time,
    temperature,
    conductance,
    temperature_coefficient
};

PhysicalDimension classify_unit_dimension(std::string_view unit) noexcept {
    if (unit == "1" || unit == "unitless" || unit == "dimensionless" || unit == "-") {
        return PhysicalDimension::unitless;
    }
    // Resistance
    if (unit == "ohm" || unit == "Ohm" || unit == "mohm" || unit == "mOhm" ||
        unit == "kohm" || unit == "kOhm" || unit == "Mohm" || unit == "MOhm" ||
        unit == "Gohm" || unit == "GOhm" || unit == "Ω" || unit == "mΩ" ||
        unit == "kΩ" || unit == "MΩ" || unit == "GΩ") {
        return PhysicalDimension::resistance;
    }
    // Capacitance
    if (unit == "F" || unit == "fF" || unit == "pF" || unit == "nF" ||
        unit == "uF" || unit == "µF" || unit == "mF") {
        return PhysicalDimension::capacitance;
    }
    // Inductance
    if (unit == "H" || unit == "pH" || unit == "nH" || unit == "uH" ||
        unit == "µH" || unit == "mH") {
        return PhysicalDimension::inductance;
    }
    // Voltage
    if (unit == "V" || unit == "nV" || unit == "uV" || unit == "µV" ||
        unit == "mV" || unit == "kV" || unit == "MV") {
        return PhysicalDimension::voltage;
    }
    // Current
    if (unit == "A" || unit == "pA" || unit == "nA" || unit == "uA" ||
        unit == "µA" || unit == "mA" || unit == "kA") {
        return PhysicalDimension::current;
    }
    // Power
    if (unit == "W" || unit == "nW" || unit == "uW" || unit == "µW" ||
        unit == "mW" || unit == "kW" || unit == "MW") {
        return PhysicalDimension::power;
    }
    // Frequency
    if (unit == "Hz" || unit == "kHz" || unit == "MHz" || unit == "GHz" || unit == "THz") {
        return PhysicalDimension::frequency;
    }
    // Time
    if (unit == "s" || unit == "ms" || unit == "us" || unit == "µs" ||
        unit == "ns" || unit == "ps" || unit == "fs") {
        return PhysicalDimension::time;
    }
    // Temperature
    if (unit == "K" || unit == "degC" || unit == "°C") {
        return PhysicalDimension::temperature;
    }
    // Conductance
    if (unit == "S" || unit == "mS" || unit == "uS" || unit == "µS") {
        return PhysicalDimension::conductance;
    }
    // Temperature coefficient / inverse temperature
    if (unit == "1/K" || unit == "ppm/K" || unit == "ppm/degC" || unit == "1/degC") {
        return PhysicalDimension::temperature_coefficient;
    }
    return PhysicalDimension::unknown;
}

bool are_parameter_units_compatible(
    std::string_view expected_unit,
    const NormalizedParameter& param,
    std::optional<std::string_view> mapping_expression) noexcept {
    if (expected_unit.empty()) {
        return false;
    }
    if (param.unitless) {
        return classify_unit_dimension(expected_unit) == PhysicalDimension::unitless;
    }
    if (!param.normalized_si_unit.has_value() || param.normalized_si_unit->empty()) {
        return false;
    }
    const std::string_view param_unit = *param.normalized_si_unit;
    if (expected_unit == param_unit) {
        return true;
    }
    const PhysicalDimension exp_dim = classify_unit_dimension(expected_unit);
    const PhysicalDimension param_dim = classify_unit_dimension(param_unit);
    if (exp_dim != PhysicalDimension::unknown && exp_dim == param_dim) {
        return mapping_expression.has_value() && !mapping_expression->empty();
    }
    return false;
}

bool is_source_document_applicable(
    const SourceDocument& document,
    const ComponentIdentity& component) noexcept {
    if (component.part_number.empty()) {
        return false;
    }
    for (const auto& apn : document.associated_part_numbers) {
        if (apn == component.part_number) {
            return true;
        }
    }
    return false;
}

ValidationReport validate_component_identity(const ComponentIdentity& identity) {
    ValidationReport report;
    check_id_token(report, identity.id.value, "component.id", "ComponentId");
    if (!is_non_blank_single_line(identity.manufacturer)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            "component.manufacturer",
            "manufacturer must be non-empty and single-line");
    }
    if (!is_non_blank_single_line(identity.part_number)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            "component.part_number",
            "part_number must be non-empty and single-line");
    }
    if (identity.part_number_suffix.has_value() &&
        !is_non_blank_single_line(*identity.part_number_suffix)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            "component.part_number_suffix",
            "present part_number_suffix must not be blank");
    }
    if (identity.package_id.has_value() &&
        !is_non_blank_single_line(*identity.package_id)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            "component.package_id",
            "present package_id must not be blank");
    }
    if (identity.subtype.has_value() &&
        !is_non_blank_single_line(*identity.subtype)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            "component.subtype",
            "present subtype must not be blank");
    }
    return report;
}

ValidationReport validate_source_document(const SourceDocument& document) {
    ValidationReport report;
    const std::string base = "source_document[" + document.id.value + "]";
    check_id_token(report, document.id.value, base + ".id", "SourceDocumentId");

    if (!is_non_blank_single_line(document.publisher)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".publisher",
            "publisher must be non-empty");
    }
    if (!is_non_blank_single_line(document.title)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".title",
            "document title must be non-empty");
    }
    if (document.associated_part_numbers.empty()) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".associated_part_numbers",
            "at least one associated part number is required");
    } else {
        std::set<std::string> seen_part_numbers;
        for (std::size_t i = 0; i < document.associated_part_numbers.size(); ++i) {
            const auto& apn = document.associated_part_numbers[i];
            const std::string apath =
                base + ".associated_part_numbers[" + std::to_string(i) + "]";
            if (!is_non_blank_single_line(apn)) {
                add_issue(
                    report,
                    ValidationRuleCode::empty_required_field,
                    apath,
                    "associated part number must not be blank");
            } else if (!seen_part_numbers.insert(apn).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    apath,
                    "duplicate associated part number in source document");
            }
        }
    }
    if (!is_non_blank_single_line(document.revision)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".revision",
            "document revision must be non-empty");
    }
    if (document.original_url.has_value() && !is_valid_identifier_token(*document.original_url)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".original_url",
            "present original_url must be non-empty and free of whitespace");
    }
    if (document.retrieval_date.has_value() &&
        !is_valid_iso8601_date_or_timestamp(*document.retrieval_date)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_date_or_timestamp,
            base + ".retrieval_date",
            "retrieval_date must be a valid ISO-8601 date or timestamp");
    }
    if (document.content_sha256.has_value() &&
        !is_valid_sha256_hex(*document.content_sha256)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_sha256_digest,
            base + ".content_sha256",
            "content_sha256 must be a 64-character hexadecimal SHA-256 digest");
    }
    if (document.usage_rights.license_identifier.has_value() &&
        !is_non_blank_single_line(*document.usage_rights.license_identifier)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".usage_rights.license_identifier",
            "present license_identifier must not be blank");
    }
    if (!is_non_blank_prose(document.usage_rights.restriction_notes)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".usage_rights.restriction_notes",
            "usage_rights.restriction_notes must be non-empty");
    }
    return report;
}

ValidationReport validate_evidence_location(const EvidenceLocation& location) {
    ValidationReport report;
    const std::string base = "evidence_location[" + location.id.value + "]";
    check_id_token(report, location.id.value, base + ".id", "EvidenceId");
    check_id_token(
        report,
        location.source_document_id.value,
        base + ".source_document_id",
        "SourceDocumentId");

    const bool has_locator = location.pdf_page_index.has_value() ||
                             location.printed_page_label.has_value() ||
                             location.section.has_value() ||
                             location.table_or_figure_id.has_value() ||
                             location.cell_or_region.has_value();
    if (!has_locator) {
        add_issue(
            report,
            ValidationRuleCode::empty_evidence_locator,
            base,
            "evidence location must specify at least one page, section, table/figure, or region");
    }
    if (location.printed_page_label.has_value() &&
        !is_non_blank_single_line(*location.printed_page_label)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".printed_page_label",
            "present printed_page_label must not be blank");
    }
    if (location.section.has_value() && !is_non_blank_single_line(*location.section)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".section",
            "present section must not be blank");
    }
    if (location.table_or_figure_id.has_value() &&
        !is_non_blank_single_line(*location.table_or_figure_id)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".table_or_figure_id",
            "present table_or_figure_id must not be blank");
    }
    if (location.cell_or_region.has_value() &&
        !is_non_blank_single_line(*location.cell_or_region)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".cell_or_region",
            "present cell_or_region must not be blank");
    }
    for (std::size_t i = 0; i < location.footnote_refs.size(); ++i) {
        if (!is_non_blank_single_line(location.footnote_refs[i])) {
            add_issue(
                report,
                ValidationRuleCode::empty_required_field,
                base + ".footnote_refs[" + std::to_string(i) + "]",
                "footnote reference must not be blank");
        }
    }
    return report;
}

ValidationReport validate_operating_condition(
    const OperatingCondition& condition,
    std::string_view path_prefix) {
    ValidationReport report;
    const std::string base(path_prefix);
    if (!is_non_blank_single_line(condition.parameter_name)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".parameter_name",
            "operating condition parameter_name must be non-empty");
    }
    if (!is_non_blank_single_line(condition.original_value_text)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".original_value_text",
            "operating condition original_value_text must be non-empty");
    }
    if (!is_non_blank_single_line(condition.unit_symbol)) {
        add_issue(
            report,
            ValidationRuleCode::missing_required_unit,
            base + ".unit_symbol",
            "operating condition must retain an explicit non-empty unit_symbol");
    }
    if (condition.numeric_value.has_value() && !std::isfinite(*condition.numeric_value)) {
        add_issue(
            report,
            ValidationRuleCode::non_finite_numeric_value,
            base + ".numeric_value",
            "operating condition numeric_value must be finite");
    }
    if (condition.notes.has_value() && !is_non_blank_prose(*condition.notes)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".notes",
            "present operating condition notes must not be blank");
    }
    return report;
}

ValidationReport validate_review_decision(
    const ReviewDecision& decision,
    std::string_view path_prefix) {
    ValidationReport report;
    const std::string base(path_prefix);
    check_id_token(report, decision.id.value, base + ".id", "ReviewDecisionId");

    if (!is_non_blank_single_line(decision.reviewer.id)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".reviewer.id",
            "reviewer identity must not be empty");
    } else if (!is_valid_identifier_token(decision.reviewer.id)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_identifier,
            base + ".reviewer.id",
            "reviewer.id must not contain whitespace or control characters");
    }
    if (decision.reviewer.display_name.has_value() &&
        !is_non_blank_single_line(*decision.reviewer.display_name)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".reviewer.display_name",
            "present reviewer display_name must not be blank");
    }

    if (decision.reviewer.kind == ActorKind::llm_assistant ||
        decision.reviewer.kind == ActorKind::automatic_parser) {
        add_issue(
            report,
            ValidationRuleCode::unauthorized_reviewer_kind,
            base + ".reviewer.kind",
            "LLM assistants and automatic parsers are prohibited from issuing review decisions");
    } else if (requires_human_engineer_reviewer(decision.new_state) &&
               decision.reviewer.kind != ActorKind::human_engineer) {
        add_issue(
            report,
            ValidationRuleCode::unauthorized_reviewer_kind,
            base + ".reviewer.kind",
            "approval, rejection, and supersession require a human_engineer reviewer");
    }

    if (!is_non_blank_single_line(decision.timestamp)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".timestamp",
            "review timestamp must not be empty");
    } else if (!is_valid_iso8601_date_or_timestamp(decision.timestamp)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_date_or_timestamp,
            base + ".timestamp",
            "review timestamp must be a valid ISO-8601 date or timestamp");
    }

    if (!is_non_blank_prose(decision.rationale)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".rationale",
            "review decision rationale must not be empty");
    }

    if (decision.evidence_refs.empty()) {
        add_issue(
            report,
            ValidationRuleCode::missing_evidence_reference,
            base + ".evidence_refs",
            "review decision must reference at least one EvidenceId");
    } else {
        std::set<EvidenceId> seen_ev;
        for (std::size_t i = 0; i < decision.evidence_refs.size(); ++i) {
            const auto& ev = decision.evidence_refs[i];
            check_id_token(
                report,
                ev.value,
                base + ".evidence_refs[" + std::to_string(i) + "]",
                "EvidenceId");
            if (!ev.empty() && !seen_ev.insert(ev).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    base + ".evidence_refs[" + std::to_string(i) + "]",
                    "duplicate EvidenceId in review decision");
            }
        }
    }

    if (!is_valid_review_transition(decision.previous_state, decision.new_state)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_review_transition,
            base + ".transition",
            "invalid review state transition");
    }

    if (decision.new_state == ReviewState::superseded) {
        if (!decision.superseding_observation_id.has_value() ||
            decision.superseding_observation_id->empty()) {
            add_issue(
                report,
                ValidationRuleCode::missing_superseding_reference,
                base + ".superseding_observation_id",
                "superseded review decision must identify a superseding ObservationId");
        } else if (!is_valid_identifier_token(decision.superseding_observation_id->value)) {
            add_issue(
                report,
                ValidationRuleCode::invalid_identifier,
                base + ".superseding_observation_id",
                "superseding_observation_id contains invalid characters");
        }
    } else if (decision.superseding_observation_id.has_value()) {
        add_issue(
            report,
            ValidationRuleCode::unexpected_superseding_reference,
            base + ".superseding_observation_id",
            "superseding_observation_id is permitted only when transitioning to superseded");
    }

    return report;
}

ValidationReport validate_parameter_observation(const ParameterObservation& observation) {
    ValidationReport report;
    const std::string base = "observation[" + observation.id.value + "]";
    check_id_token(report, observation.id.value, base + ".id", "ObservationId");
    check_id_token(
        report,
        observation.component_id.value,
        base + ".component_id",
        "ComponentId");

    if (!is_non_blank_single_line(observation.parameter_name)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".parameter_name",
            "parameter_name must be non-empty");
    }

    if (observation.original_value_text.has_value() &&
        !is_non_blank_single_line(*observation.original_value_text)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".original_value_text",
            "present original_value_text must not be blank; use std::nullopt for missing values");
    }

    if (observation.original_unit_symbol.has_value() &&
        !is_non_blank_single_line(*observation.original_unit_symbol)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".original_unit_symbol",
            "present original_unit_symbol must not be blank");
    }

    if (observation.unitless && observation.original_unit_symbol.has_value()) {
        add_issue(
            report,
            ValidationRuleCode::contradictory_unitless_flag,
            base + ".unitless",
            "unitless observation cannot also specify an original_unit_symbol");
    }

    if (observation.parsed_numeric_value.has_value()) {
        if (!std::isfinite(*observation.parsed_numeric_value)) {
            add_issue(
                report,
                ValidationRuleCode::non_finite_numeric_value,
                base + ".parsed_numeric_value",
                "parsed_numeric_value must be finite");
        }
        if (!observation.original_value_text.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::missing_numeric_source_text,
                base + ".original_value_text",
                "parsed_numeric_value requires preserved original_value_text");
        }
        if (!observation.unitless && !observation.original_unit_symbol.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::missing_required_unit,
                base + ".original_unit_symbol",
                "numeric observation requiring a unit must specify original_unit_symbol");
        }
    }

    for (std::size_t i = 0; i < observation.operating_conditions.size(); ++i) {
        append_report(
            report,
            validate_operating_condition(
                observation.operating_conditions[i],
                base + ".operating_conditions[" + std::to_string(i) + "]"));
    }

    if (observation.evidence_refs.empty()) {
        add_issue(
            report,
            ValidationRuleCode::missing_evidence_reference,
            base + ".evidence_refs",
            "observation must reference at least one EvidenceLocation");
    } else {
        std::set<EvidenceId> seen_ev;
        for (std::size_t i = 0; i < observation.evidence_refs.size(); ++i) {
            const auto& ev = observation.evidence_refs[i];
            check_id_token(
                report,
                ev.value,
                base + ".evidence_refs[" + std::to_string(i) + "]",
                "EvidenceId");
            if (!ev.empty() && !seen_ev.insert(ev).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    base + ".evidence_refs[" + std::to_string(i) + "]",
                    "duplicate EvidenceId on observation");
            }
        }
    }

    if (requires_extraction_tool(observation.extraction_method) &&
        (!observation.extraction_tool.has_value() ||
         !is_non_blank_single_line(*observation.extraction_tool))) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".extraction_tool",
            "automated extraction_method requires a non-empty extraction_tool identifier");
    }
    if (observation.extraction_tool.has_value() &&
        !is_non_blank_single_line(*observation.extraction_tool)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".extraction_tool",
            "present extraction_tool must not be blank");
    }
    if (observation.extraction_tool_version.has_value() &&
        !is_non_blank_single_line(*observation.extraction_tool_version)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".extraction_tool_version",
            "present extraction_tool_version must not be blank");
    }
    if (observation.extracted_at.has_value() &&
        !is_valid_iso8601_date_or_timestamp(*observation.extracted_at)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_date_or_timestamp,
            base + ".extracted_at",
            "extracted_at must be a valid ISO-8601 date or timestamp");
    }
    if (observation.uncertainty_note.has_value() &&
        !is_non_blank_prose(*observation.uncertainty_note)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".uncertainty_note",
            "present uncertainty_note must not be blank");
    }

    std::set<ReviewDecisionId> seen_decisions;
    for (std::size_t i = 0; i < observation.review_history.size(); ++i) {
        const auto& dec = observation.review_history[i];
        const std::string dec_path = base + ".review_history[" + std::to_string(i) + "]";
        append_report(report, validate_review_decision(dec, dec_path));
        if (!dec.id.empty() && !seen_decisions.insert(dec.id).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                dec_path + ".id",
                "duplicate ReviewDecisionId in observation review_history");
        }
        if (observation.extraction_tool.has_value() &&
            !dec.reviewer.id.empty() &&
            *observation.extraction_tool == dec.reviewer.id) {
            add_issue(
                report,
                ValidationRuleCode::self_approval_prohibited,
                dec_path + ".reviewer.id",
                "extraction_tool may not review or approve its own extracted observation");
        }
        if (dec.superseding_observation_id.has_value() &&
            *dec.superseding_observation_id == observation.id) {
            add_issue(
                report,
                ValidationRuleCode::invalid_superseding_target,
                dec_path + ".superseding_observation_id",
                "observation cannot supersede itself");
        }
        if (dec.new_state == ReviewState::approved &&
            (!observation.original_value_text.has_value() ||
             !is_non_blank_single_line(*observation.original_value_text))) {
            add_issue(
                report,
                ValidationRuleCode::missing_value_cannot_be_approved,
                dec_path,
                "observation with missing original_value_text cannot be approved");
        }
    }

    if (observation.review_state == ReviewState::candidate) {
        if (!observation.review_history.empty()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_review_history,
                base + ".review_history",
                "candidate observation must have an empty review_history");
        }
        if (observation.superseded_by.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::unexpected_superseding_reference,
                base + ".superseded_by",
                "candidate observation cannot set superseded_by");
        }
    } else {
        if (observation.review_history.empty()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_review_history,
                base + ".review_history",
                "non-candidate observation requires a complete review_history");
        } else {
            if (observation.review_history.front().previous_state != ReviewState::candidate) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_review_history,
                    base + ".review_history[0].previous_state",
                    "review_history must start from ReviewState::candidate");
            }
            for (std::size_t i = 1; i < observation.review_history.size(); ++i) {
                if (observation.review_history[i].previous_state !=
                    observation.review_history[i - 1U].new_state) {
                    add_issue(
                        report,
                        ValidationRuleCode::invalid_review_history,
                        base + ".review_history[" + std::to_string(i) + "].previous_state",
                        "discontinuous state chain in review_history");
                }
            }
            if (observation.review_history.back().new_state != observation.review_state) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_review_history,
                    base + ".review_state",
                    "observation review_state does not match final review_history decision");
            }
        }

        if (observation.review_state == ReviewState::approved &&
            (!observation.original_value_text.has_value() ||
             !is_non_blank_single_line(*observation.original_value_text))) {
            add_issue(
                report,
                ValidationRuleCode::missing_value_cannot_be_approved,
                base + ".original_value_text",
                "approved observation must have preserved original_value_text");
        }

        if (observation.review_state == ReviewState::superseded) {
            if (!observation.superseded_by.has_value() || observation.superseded_by->empty()) {
                add_issue(
                    report,
                    ValidationRuleCode::missing_superseding_reference,
                    base + ".superseded_by",
                    "superseded observation must identify its replacement in superseded_by");
            } else {
                check_id_token(
                    report,
                    observation.superseded_by->value,
                    base + ".superseded_by",
                    "ObservationId");
                if (*observation.superseded_by == observation.id) {
                    add_issue(
                        report,
                        ValidationRuleCode::invalid_superseding_target,
                        base + ".superseded_by",
                        "observation cannot supersede itself");
                }
                if (!observation.review_history.empty() &&
                    observation.review_history.back().superseding_observation_id !=
                        observation.superseded_by) {
                    add_issue(
                        report,
                        ValidationRuleCode::invalid_review_history,
                        base + ".superseded_by",
                        "superseded_by must match the final review decision superseding_observation_id");
                }
            }
        } else if (observation.superseded_by.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::unexpected_superseding_reference,
                base + ".superseded_by",
                "superseded_by is permitted only when review_state is superseded");
        }
    }

    return report;
}

ValidationReport validate_normalized_parameter(const NormalizedParameter& parameter) {
    ValidationReport report;
    const std::string base = "normalized_parameter[" + parameter.id.value + "]";
    check_id_token(report, parameter.id.value, base + ".id", "NormalizedParameterId");
    check_id_token(
        report,
        parameter.source_observation_id.value,
        base + ".source_observation_id",
        "ObservationId");

    if (!is_non_blank_single_line(parameter.canonical_name)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".canonical_name",
            "canonical_name must be non-empty");
    }
    if (!is_non_blank_prose(parameter.quantity_description)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".quantity_description",
            "quantity_description must be non-empty");
    }
    if (!is_non_blank_single_line(parameter.preserved_original_value_text)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".preserved_original_value_text",
            "preserved_original_value_text must be non-empty");
    }
    if (parameter.preserved_original_unit_symbol.has_value() &&
        !is_non_blank_single_line(*parameter.preserved_original_unit_symbol)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".preserved_original_unit_symbol",
            "present preserved_original_unit_symbol must not be blank");
    }
    if (!is_non_blank_prose(parameter.conversion_rule)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".conversion_rule",
            "explicit conversion_rule description is required");
    }
    if (parameter.normalized_si_unit.has_value() &&
        !is_non_blank_single_line(*parameter.normalized_si_unit)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".normalized_si_unit",
            "present normalized_si_unit must not be blank");
    }
    if (parameter.unitless && parameter.normalized_si_unit.has_value()) {
        add_issue(
            report,
            ValidationRuleCode::contradictory_unitless_flag,
            base + ".unitless",
            "unitless normalized parameter cannot also carry normalized_si_unit");
    }
    if (parameter.normalized_numeric_value.has_value()) {
        if (!std::isfinite(*parameter.normalized_numeric_value)) {
            add_issue(
                report,
                ValidationRuleCode::non_finite_numeric_value,
                base + ".normalized_numeric_value",
                "normalized_numeric_value must be finite");
        }
        if (!parameter.unitless && !parameter.normalized_si_unit.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::missing_required_unit,
                base + ".normalized_si_unit",
                "numeric normalized parameter requiring a unit must specify normalized_si_unit");
        }
    }
    for (std::size_t i = 0; i < parameter.applicable_conditions.size(); ++i) {
        append_report(
            report,
            validate_operating_condition(
                parameter.applicable_conditions[i],
                base + ".applicable_conditions[" + std::to_string(i) + "]"));
    }
    return report;
}

ValidationReport validate_component_knowledge(const ComponentKnowledge& knowledge) {
    ValidationReport report;
    const std::string base = "component_knowledge[" + knowledge.id.value + "]";
    check_id_token(report, knowledge.id.value, base + ".id", "ComponentKnowledgeId");
    append_report(report, validate_component_identity(knowledge.component));

    if (knowledge.version == 0U) {
        add_issue(
            report,
            ValidationRuleCode::invalid_version_history,
            base + ".version",
            "ComponentKnowledge version must be >= 1");
    }
    if (knowledge.version == 1U && knowledge.previous_version_id.has_value()) {
        add_issue(
            report,
            ValidationRuleCode::invalid_version_history,
            base + ".previous_version_id",
            "version 1 ComponentKnowledge must not specify previous_version_id");
    }
    if (knowledge.version > 1U) {
        if (!knowledge.previous_version_id.has_value() ||
            knowledge.previous_version_id->empty()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_version_history,
                base + ".previous_version_id",
                "ComponentKnowledge version > 1 must specify previous_version_id");
        } else {
            check_id_token(
                report,
                knowledge.previous_version_id->value,
                base + ".previous_version_id",
                "ComponentKnowledgeId");
            if (*knowledge.previous_version_id == knowledge.id) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_version_history,
                    base + ".previous_version_id",
                    "previous_version_id must differ from current knowledge id");
            }
        }
    }

    append_report(
        report,
        validate_review_decision(knowledge.review_decision, base + ".review_decision"));
    if (knowledge.review_decision.new_state != ReviewState::approved) {
        add_issue(
            report,
            ValidationRuleCode::invalid_review_history,
            base + ".review_decision.new_state",
            "ComponentKnowledge review_decision must approve the knowledge snapshot");
    }

    if (knowledge.version_history.empty()) {
        add_issue(
            report,
            ValidationRuleCode::invalid_version_history,
            base + ".version_history",
            "ComponentKnowledge must retain at least one version_history entry");
    } else {
        std::set<ComponentKnowledgeId> seen_hist_ids;
        for (std::size_t i = 0; i < knowledge.version_history.size(); ++i) {
            const auto& entry = knowledge.version_history[i];
            const std::string epath = base + ".version_history[" + std::to_string(i) + "]";
            check_id_token(report, entry.knowledge_id.value, epath + ".knowledge_id", "ComponentKnowledgeId");
            if (!entry.knowledge_id.empty() && !seen_hist_ids.insert(entry.knowledge_id).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    epath + ".knowledge_id",
                    "duplicate knowledge_id in version_history");
            }
            if (entry.version != static_cast<std::uint32_t>(i + 1U)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_version_history,
                    epath + ".version",
                    "version_history entries must be sequential starting at 1");
            }
            if (i == 0U && entry.previous_knowledge_id.has_value()) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_version_history,
                    epath + ".previous_knowledge_id",
                    "initial version_history entry must not have previous_knowledge_id");
            }
            if (i > 0U && entry.previous_knowledge_id != knowledge.version_history[i - 1U].knowledge_id) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_version_history,
                    epath + ".previous_knowledge_id",
                    "version_history previous_knowledge_id must link to preceding revision");
            }
            if (!is_valid_identifier_token(entry.reviewer.id) ||
                entry.reviewer.kind != ActorKind::human_engineer) {
                add_issue(
                    report,
                    ValidationRuleCode::unauthorized_reviewer_kind,
                    epath + ".reviewer",
                    "version_history entry requires a valid human_engineer reviewer");
            }
            if (!is_valid_iso8601_date_or_timestamp(entry.timestamp)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_date_or_timestamp,
                    epath + ".timestamp",
                    "version_history timestamp must be valid ISO-8601");
            }
            if (!is_non_blank_prose(entry.summary)) {
                add_issue(
                    report,
                    ValidationRuleCode::empty_required_field,
                    epath + ".summary",
                    "version_history summary must not be empty");
            }
        }
        const auto& last = knowledge.version_history.back();
        if (last.version != knowledge.version ||
            last.knowledge_id != knowledge.id ||
            last.previous_knowledge_id != knowledge.previous_version_id) {
            add_issue(
                report,
                ValidationRuleCode::invalid_version_history,
                base + ".version_history",
                "final version_history entry must match current ComponentKnowledge version and id");
        }
    }

    std::set<ObservationId> approved_set;
    for (std::size_t i = 0; i < knowledge.approved_observation_refs.size(); ++i) {
        const auto& obs_id = knowledge.approved_observation_refs[i];
        check_id_token(
            report,
            obs_id.value,
            base + ".approved_observation_refs[" + std::to_string(i) + "]",
            "ObservationId");
        if (!obs_id.empty() && !approved_set.insert(obs_id).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                base + ".approved_observation_refs[" + std::to_string(i) + "]",
                "duplicate ObservationId in approved_observation_refs");
        }
    }

    std::set<std::string> conflict_ids;
    std::set<ObservationId> blocked_from_promotion;
    for (std::size_t i = 0; i < knowledge.conflicts.size(); ++i) {
        const auto& conflict = knowledge.conflicts[i];
        const std::string cpath = base + ".conflicts[" + std::to_string(i) + "]";
        check_id_token(report, conflict.conflict_id, cpath + ".conflict_id", "conflict_id");
        if (!conflict.conflict_id.empty() && !conflict_ids.insert(conflict.conflict_id).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                cpath + ".conflict_id",
                "duplicate conflict_id in ComponentKnowledge");
        }
        if (!is_non_blank_single_line(conflict.parameter_name)) {
            add_issue(
                report,
                ValidationRuleCode::empty_required_field,
                cpath + ".parameter_name",
                "conflict parameter_name must not be empty");
        }
        if (!is_non_blank_prose(conflict.description)) {
            add_issue(
                report,
                ValidationRuleCode::empty_required_field,
                cpath + ".description",
                "conflict description must not be empty");
        }
        if (conflict.competing_observations.size() < 2U) {
            add_issue(
                report,
                ValidationRuleCode::invalid_conflict_record,
                cpath + ".competing_observations",
                "ParameterConflict must list at least two competing observations");
        }
        std::set<ObservationId> competing_set;
        for (std::size_t j = 0; j < conflict.competing_observations.size(); ++j) {
            const auto& cid = conflict.competing_observations[j];
            check_id_token(
                report,
                cid.value,
                cpath + ".competing_observations[" + std::to_string(j) + "]",
                "ObservationId");
            if (!cid.empty() && !competing_set.insert(cid).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    cpath + ".competing_observations[" + std::to_string(j) + "]",
                    "duplicate ObservationId inside competing_observations");
            }
        }
        if (conflict.status == ConflictResolutionStatus::unresolved) {
            if (conflict.selected_observation_id.has_value()) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_conflict_record,
                    cpath + ".selected_observation_id",
                    "unresolved conflict must not set selected_observation_id");
            }
            for (const auto& cid : competing_set) {
                blocked_from_promotion.insert(cid);
            }
        } else {
            if (!conflict.selected_observation_id.has_value() ||
                competing_set.find(*conflict.selected_observation_id) == competing_set.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_conflict_record,
                    cpath + ".selected_observation_id",
                    "resolved conflict must select one of its competing_observations");
            }
            if (!conflict.reviewer.has_value() ||
                !is_valid_identifier_token(conflict.reviewer->id) ||
                conflict.reviewer->kind != ActorKind::human_engineer) {
                add_issue(
                    report,
                    ValidationRuleCode::unauthorized_reviewer_kind,
                    cpath + ".reviewer",
                    "resolved conflict requires a human_engineer reviewer");
            }
            if (!conflict.resolution_rationale.has_value() ||
                !is_non_blank_prose(*conflict.resolution_rationale)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_conflict_record,
                    cpath + ".resolution_rationale",
                    "resolved conflict requires a non-empty resolution_rationale");
            }
            for (const auto& cid : competing_set) {
                if (!conflict.selected_observation_id.has_value() ||
                    cid != *conflict.selected_observation_id) {
                    blocked_from_promotion.insert(cid);
                }
            }
        }
    }

    std::set<NormalizedParameterId> param_ids;
    using SlotKey = std::tuple<std::string, ParameterCategory, ValueQualifier, ConditionKey>;
    std::map<SlotKey, const NormalizedParameter*> occupied_slots;

    using BoundGroupKey = std::tuple<std::string, ParameterCategory, std::string, ConditionKey>;
    struct QualifierBounds {
        std::optional<double> min_val;
        std::optional<double> typ_val;
        std::optional<double> max_val;
    };
    std::map<BoundGroupKey, QualifierBounds> bound_groups;

    for (std::size_t i = 0; i < knowledge.normalized_parameters.size(); ++i) {
        const auto& param = knowledge.normalized_parameters[i];
        const std::string ppath = base + ".normalized_parameters[" + std::to_string(i) + "]";
        append_report(report, validate_normalized_parameter(param));

        if (!param.id.empty() && !param_ids.insert(param.id).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                ppath + ".id",
                "duplicate NormalizedParameterId in ComponentKnowledge");
        }
        if (approved_set.find(param.source_observation_id) == approved_set.end()) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                ppath + ".source_observation_id",
                "NormalizedParameter source_observation_id must be listed in approved_observation_refs");
        }
        if (blocked_from_promotion.find(param.source_observation_id) !=
            blocked_from_promotion.end()) {
            add_issue(
                report,
                ValidationRuleCode::conflicted_observation_promoted,
                ppath + ".source_observation_id",
                "unresolved or non-selected conflicting observation cannot be promoted");
        }

        const ConditionKey ckey = canonical_condition_key(param.applicable_conditions);
        const SlotKey skey{param.canonical_name, param.category, param.qualifier, ckey};
        const auto [it, inserted] = occupied_slots.emplace(skey, &param);
        if (!inserted) {
            add_issue(
                report,
                ValidationRuleCode::undeclared_parameter_conflict,
                ppath,
                "multiple normalized parameters occupy identical canonical/category/qualifier/condition slot");
        }

        if (param.normalized_numeric_value.has_value() &&
            std::isfinite(*param.normalized_numeric_value)) {
            const std::string unit_key = param.unitless
                                             ? std::string("<unitless>")
                                             : param.normalized_si_unit.value_or("");
            const BoundGroupKey bkey{param.canonical_name, param.category, unit_key, ckey};
            auto& bounds = bound_groups[bkey];
            if (param.qualifier == ValueQualifier::minimum) {
                bounds.min_val = param.normalized_numeric_value;
            } else if (param.qualifier == ValueQualifier::typical) {
                bounds.typ_val = param.normalized_numeric_value;
            } else if (param.qualifier == ValueQualifier::maximum) {
                bounds.max_val = param.normalized_numeric_value;
            }
        }
    }

    for (const auto& [bkey, bounds] : bound_groups) {
        const auto& cname = std::get<0>(bkey);
        if ((bounds.min_val.has_value() && bounds.typ_val.has_value() &&
             *bounds.min_val > *bounds.typ_val) ||
            (bounds.typ_val.has_value() && bounds.max_val.has_value() &&
             *bounds.typ_val > *bounds.max_val) ||
            (bounds.min_val.has_value() && bounds.max_val.has_value() &&
             *bounds.min_val > *bounds.max_val)) {
            add_issue(
                report,
                ValidationRuleCode::reversed_qualifier_bounds,
                base + ".normalized_parameters[" + cname + "]",
                "minimum/typical/maximum normalized bounds are reversed under identical conditions");
        }
    }

    return report;
}

ValidationReport validate_model_parameter_mapping(
    const ModelParameterMapping& mapping,
    const NormalizedParameter* resolved_param,
    std::string_view path_prefix) {
    ValidationReport report;
    const std::string base(path_prefix);

    if (!is_non_blank_single_line(mapping.model_parameter_name)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_parameter_mapping,
            base + ".model_parameter_name",
            "model_parameter_name must not be empty");
    }

    if (!is_non_blank_single_line(mapping.expected_unit)) {
        add_issue(
            report,
            ValidationRuleCode::missing_required_unit,
            base + ".expected_unit",
            "model parameter mapping requires an explicit expected_unit");
    }

    if (mapping.knowledge_param_id.has_value() == mapping.fixed_value.has_value()) {
        add_issue(
            report,
            ValidationRuleCode::invalid_parameter_mapping,
            base,
            "parameter mapping must specify either knowledge_param_id or fixed_value (exclusively)");
    }

    if (mapping.knowledge_param_id.has_value()) {
        check_id_token(
            report,
            mapping.knowledge_param_id->value,
            base + ".knowledge_param_id",
            "NormalizedParameterId");
    }

    if (mapping.fixed_value.has_value() && !std::isfinite(*mapping.fixed_value)) {
        add_issue(
            report,
            ValidationRuleCode::non_finite_numeric_value,
            base + ".fixed_value",
            "parameter mapping fixed_value must be finite");
    }

    if (mapping.mapping_expression.has_value() && !is_non_blank_single_line(*mapping.mapping_expression)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_parameter_mapping,
            base + ".mapping_expression",
            "present mapping_expression must not be blank");
    }

    if (resolved_param != nullptr) {
        if (!are_parameter_units_compatible(
                mapping.expected_unit,
                *resolved_param,
                mapping.mapping_expression)) {
            const std::string resolved_unit_str = resolved_param->unitless
                ? std::string("unitless")
                : resolved_param->normalized_si_unit.value_or(std::string("<none>"));
            add_issue(
                report,
                ValidationRuleCode::incompatible_parameter_units,
                base + ".expected_unit",
                "expected_unit '" + mapping.expected_unit + "' is incompatible with normalized parameter unit '" +
                    resolved_unit_str + "'");
        }
    }

    return report;
}

ValidationReport validate_model_artifact_descriptor(const ModelArtifactDescriptor& descriptor) {
    ValidationReport report;
    const std::string base = "model_artifact[" + descriptor.id.value + "]";
    check_id_token(report, descriptor.id.value, base + ".id", "ModelArtifactId");
    check_id_token(
        report,
        descriptor.component_id.value,
        base + ".component_id",
        "ComponentId");
    if (descriptor.component_knowledge_id.has_value()) {
        check_id_token(
            report,
            descriptor.component_knowledge_id->value,
            base + ".component_knowledge_id",
            "ComponentKnowledgeId");
    }

    if (!is_valid_version_token(descriptor.version)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_model_version,
            base + ".version",
            "model version must be non-empty and free of whitespace or control characters");
    }

    if (!is_non_blank_single_line(descriptor.source_vendor)) {
        add_issue(
            report,
            ValidationRuleCode::missing_model_source_or_license,
            base + ".source_vendor",
            "model source_vendor is required");
    }

    if (!is_non_blank_single_line(descriptor.license.license_identifier) ||
        !is_non_blank_prose(descriptor.license.usage_restrictions)) {
        add_issue(
            report,
            ValidationRuleCode::missing_model_source_or_license,
            base + ".license",
            "model license_identifier and usage_restrictions are required");
    }

    if (requires_external_artifact_uri(descriptor.format) &&
        (!descriptor.artifact_uri.has_value() ||
         !is_valid_identifier_token(*descriptor.artifact_uri))) {
        add_issue(
            report,
            ValidationRuleCode::missing_artifact_reference,
            base + ".artifact_uri",
            "non-analytical model format requires an explicit external artifact_uri");
    } else if (descriptor.artifact_uri.has_value() &&
               !is_valid_identifier_token(*descriptor.artifact_uri)) {
        add_issue(
            report,
            ValidationRuleCode::missing_artifact_reference,
            base + ".artifact_uri",
            "present artifact_uri must be non-empty and free of whitespace");
    }

    if (descriptor.artifact_sha256.has_value() &&
        !is_valid_sha256_hex(*descriptor.artifact_sha256)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_sha256_digest,
            base + ".artifact_sha256",
            "artifact_sha256 must be a 64-character hexadecimal SHA-256 digest");
    }

    if (descriptor.terminals.size() < 2U) {
        add_issue(
            report,
            ValidationRuleCode::invalid_terminal_mapping,
            base + ".terminals",
            "electrical component model must declare at least two ordered terminals");
    } else {
        std::set<std::string> terminal_names;
        std::set<std::string> pin_ids;
        for (std::size_t i = 0; i < descriptor.terminals.size(); ++i) {
            const auto& term = descriptor.terminals[i];
            const std::string tpath = base + ".terminals[" + std::to_string(i) + "]";
            if (term.position != i + 1U) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_terminal_mapping,
                    tpath + ".position",
                    "terminal positions must be explicitly ordered 1..N");
            }
            if (!is_non_blank_single_line(term.terminal_name)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_terminal_mapping,
                    tpath + ".terminal_name",
                    "terminal_name must not be empty");
            } else if (!terminal_names.insert(term.terminal_name).second) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_terminal_mapping,
                    tpath + ".terminal_name",
                    "duplicate terminal_name in model terminal mapping");
            }
            if (!is_non_blank_single_line(term.physical_pin_id)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_terminal_mapping,
                    tpath + ".physical_pin_id",
                    "physical_pin_id must not be empty");
            } else if (!pin_ids.insert(term.physical_pin_id).second) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_terminal_mapping,
                    tpath + ".physical_pin_id",
                    "duplicate physical_pin_id in model terminal mapping");
            }
            if (term.description.has_value() && !is_non_blank_prose(*term.description)) {
                add_issue(
                    report,
                    ValidationRuleCode::empty_required_field,
                    tpath + ".description",
                    "present terminal description must not be blank");
            }
        }
    }

    std::set<std::string> mapped_names;
    for (std::size_t i = 0; i < descriptor.parameter_mappings.size(); ++i) {
        const auto& pm = descriptor.parameter_mappings[i];
        const std::string ppath = base + ".parameter_mappings[" + std::to_string(i) + "]";
        append_report(report, validate_model_parameter_mapping(pm, nullptr, ppath));
        if (!pm.model_parameter_name.empty() && !mapped_names.insert(pm.model_parameter_name).second) {
            add_issue(
                report,
                ValidationRuleCode::invalid_parameter_mapping,
                ppath + ".model_parameter_name",
                "duplicate model_parameter_name in parameter_mappings");
        }
        if (pm.knowledge_param_id.has_value() && !descriptor.component_knowledge_id.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_parameter_mapping,
                ppath + ".knowledge_param_id",
                "knowledge-backed parameter mapping requires descriptor.component_knowledge_id");
        }
    }

    if (descriptor.validity_envelope.operating_ranges.empty()) {
        add_issue(
            report,
            ValidationRuleCode::invalid_validity_envelope,
            base + ".validity_envelope.operating_ranges",
            "validity_envelope must define at least one operating range constraint");
    } else {
        for (std::size_t i = 0; i < descriptor.validity_envelope.operating_ranges.size(); ++i) {
            const auto& r = descriptor.validity_envelope.operating_ranges[i];
            const std::string rpath =
                base + ".validity_envelope.operating_ranges[" + std::to_string(i) + "]";
            if (!is_non_blank_single_line(r.quantity_name)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_validity_envelope,
                    rpath + ".quantity_name",
                    "validity range quantity_name must not be empty");
            }
            if (!is_non_blank_single_line(r.unit_symbol)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_validity_envelope,
                    rpath + ".unit_symbol",
                    "validity range unit_symbol must not be empty");
            }
            if (!r.min_value.has_value() && !r.max_value.has_value()) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_validity_envelope,
                    rpath,
                    "validity range must specify at least min_value or max_value");
            }
            if (r.min_value.has_value() && !std::isfinite(*r.min_value)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_validity_envelope,
                    rpath + ".min_value",
                    "validity range min_value must be finite");
            }
            if (r.max_value.has_value() && !std::isfinite(*r.max_value)) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_validity_envelope,
                    rpath + ".max_value",
                    "validity range max_value must be finite");
            }
            if (r.min_value.has_value() && r.max_value.has_value() &&
                std::isfinite(*r.min_value) && std::isfinite(*r.max_value)) {
                if (*r.min_value > *r.max_value ||
                    (*r.min_value == *r.max_value && (!r.min_inclusive || !r.max_inclusive))) {
                    add_issue(
                        report,
                        ValidationRuleCode::invalid_validity_envelope,
                        rpath,
                        "validity range bounds are inverted or form an empty open interval");
                }
            }
        }
    }
    for (std::size_t i = 0; i < descriptor.validity_envelope.assumptions.size(); ++i) {
        if (!is_non_blank_prose(descriptor.validity_envelope.assumptions[i])) {
            add_issue(
                report,
                ValidationRuleCode::invalid_validity_envelope,
                base + ".validity_envelope.assumptions[" + std::to_string(i) + "]",
                "validity assumption must not be blank");
        }
    }
    if (descriptor.validity_envelope.notes.has_value() &&
        !is_non_blank_prose(*descriptor.validity_envelope.notes)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_validity_envelope,
            base + ".validity_envelope.notes",
            "present validity_envelope notes must not be blank");
    }

    if (descriptor.supported_analyses.empty()) {
        add_issue(
            report,
            ValidationRuleCode::empty_supported_analyses,
            base + ".supported_analyses",
            "model must declare at least one supported AnalysisType");
    } else {
        std::set<AnalysisType> seen_analyses;
        for (std::size_t i = 0; i < descriptor.supported_analyses.size(); ++i) {
            if (!seen_analyses.insert(descriptor.supported_analyses[i]).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    base + ".supported_analyses[" + std::to_string(i) + "]",
                    "duplicate AnalysisType in supported_analyses");
            }
        }
    }

    if (descriptor.limitations.empty()) {
        add_issue(
            report,
            ValidationRuleCode::empty_model_limitations,
            base + ".limitations",
            "model must explicitly state at least one limitation");
    } else {
        for (std::size_t i = 0; i < descriptor.limitations.size(); ++i) {
            if (!is_non_blank_prose(descriptor.limitations[i])) {
                add_issue(
                    report,
                    ValidationRuleCode::empty_model_limitations,
                    base + ".limitations[" + std::to_string(i) + "]",
                    "model limitation entry must not be blank");
            }
        }
    }

    std::set<ModelValidationId> seen_val_refs;
    for (std::size_t i = 0; i < descriptor.validation_record_refs.size(); ++i) {
        const auto& vref = descriptor.validation_record_refs[i];
        check_id_token(
            report,
            vref.value,
            base + ".validation_record_refs[" + std::to_string(i) + "]",
            "ModelValidationId");
        if (!vref.empty() && !seen_val_refs.insert(vref).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                base + ".validation_record_refs[" + std::to_string(i) + "]",
                "duplicate ModelValidationId in validation_record_refs");
        }
    }

    return report;
}

ValidationReport validate_model_validation_record(const ModelValidationRecord& record) {
    ValidationReport report;
    const std::string base = "model_validation[" + record.id.value + "]";
    check_id_token(report, record.id.value, base + ".id", "ModelValidationId");
    check_id_token(report, record.model_id.value, base + ".model_id", "ModelArtifactId");

    if (!is_valid_version_token(record.model_version)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_model_version,
            base + ".model_version",
            "model_version in validation record must be a valid version token");
    }
    if (!is_non_blank_prose(record.summary)) {
        add_issue(
            report,
            ValidationRuleCode::empty_required_field,
            base + ".summary",
            "model validation summary must not be empty");
    }

    if (record.timestamp.has_value() && !is_valid_iso8601_date_or_timestamp(*record.timestamp)) {
        add_issue(
            report,
            ValidationRuleCode::invalid_date_or_timestamp,
            base + ".timestamp",
            "present model validation timestamp must be valid ISO-8601");
    }
    if (record.reviewer.has_value()) {
        if (!is_valid_identifier_token(record.reviewer->id)) {
            add_issue(
                report,
                ValidationRuleCode::invalid_identifier,
                base + ".reviewer.id",
                "present model validation reviewer.id is invalid");
        }
        if (record.reviewer->kind == ActorKind::llm_assistant ||
            record.reviewer->kind == ActorKind::automatic_parser) {
            add_issue(
                report,
                ValidationRuleCode::unauthorized_reviewer_kind,
                base + ".reviewer.kind",
                "LLM assistants and automatic parsers cannot author model validation records");
        }
    }

    if (record.status == ModelValidationStatus::unvalidated) {
        if (record.validation_kind != ModelValidationKind::none ||
            !record.verified_analyses.empty()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_model_validation_record,
                base,
                "unvalidated model record must have validation_kind::none and empty verified_analyses");
        }
    } else {
        if (!record.timestamp.has_value()) {
            add_issue(
                report,
                ValidationRuleCode::invalid_model_validation_record,
                base + ".timestamp",
                "non-unvalidated model record requires a timestamp");
        }
        if (record.status == ModelValidationStatus::validated_within_envelope) {
            if (record.validation_kind == ModelValidationKind::none) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_model_validation_record,
                    base + ".validation_kind",
                    "validated_within_envelope record must specify a concrete validation_kind");
            }
            if (!record.reviewer.has_value() ||
                record.reviewer->kind != ActorKind::human_engineer) {
                add_issue(
                    report,
                    ValidationRuleCode::unauthorized_reviewer_kind,
                    base + ".reviewer",
                    "validated_within_envelope status requires a human_engineer reviewer");
            }
            if (record.verified_analyses.empty()) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_model_validation_record,
                    base + ".verified_analyses",
                    "validated_within_envelope record must specify at least one verified AnalysisType");
            }
            if (record.evidence_refs.empty()) {
                add_issue(
                    report,
                    ValidationRuleCode::missing_evidence_reference,
                    base + ".evidence_refs",
                    "validated_within_envelope record requires supporting evidence_refs");
            }
        }
    }

    std::set<EvidenceId> seen_ev;
    for (std::size_t i = 0; i < record.evidence_refs.size(); ++i) {
        check_id_token(
            report,
            record.evidence_refs[i].value,
            base + ".evidence_refs[" + std::to_string(i) + "]",
            "EvidenceId");
        if (!record.evidence_refs[i].empty() && !seen_ev.insert(record.evidence_refs[i]).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                base + ".evidence_refs[" + std::to_string(i) + "]",
                "duplicate EvidenceId in model validation record");
        }
    }

    std::set<AnalysisType> seen_analyses;
    for (std::size_t i = 0; i < record.verified_analyses.size(); ++i) {
        if (!seen_analyses.insert(record.verified_analyses[i]).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                base + ".verified_analyses[" + std::to_string(i) + "]",
                "duplicate AnalysisType in verified_analyses");
        }
    }

    return report;
}

ValidationReport validate_domain_bundle(const ComponentDomainBundle& bundle) {
    ValidationReport report;
    append_report(report, validate_component_identity(bundle.component));

    std::map<SourceDocumentId, const SourceDocument*> doc_index;
    for (std::size_t i = 0; i < bundle.documents.size(); ++i) {
        const auto& doc = bundle.documents[i];
        append_report(report, validate_source_document(doc));
        if (!doc.id.empty() && !doc_index.emplace(doc.id, &doc).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                "documents[" + std::to_string(i) + "].id",
                "duplicate SourceDocumentId in bundle");
        }
        if (!is_source_document_applicable(doc, bundle.component)) {
            add_issue(
                report,
                ValidationRuleCode::incompatible_source_document_part_number,
                "documents[" + std::to_string(i) + "].associated_part_numbers",
                "source document is not applicable to component part number: " + bundle.component.part_number);
        }
    }

    std::map<EvidenceId, const EvidenceLocation*> ev_index;
    for (std::size_t i = 0; i < bundle.evidence_locations.size(); ++i) {
        const auto& ev = bundle.evidence_locations[i];
        append_report(report, validate_evidence_location(ev));
        if (!ev.id.empty() && !ev_index.emplace(ev.id, &ev).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                "evidence_locations[" + std::to_string(i) + "].id",
                "duplicate EvidenceId in bundle");
        }
        if (!ev.source_document_id.empty() &&
            doc_index.find(ev.source_document_id) == doc_index.end()) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                "evidence_location[" + ev.id.value + "].source_document_id",
                "EvidenceLocation references unknown SourceDocumentId");
        }
    }

    std::map<ObservationId, const ParameterObservation*> obs_index;
    std::set<ReviewDecisionId> all_decision_ids;
    for (std::size_t i = 0; i < bundle.observations.size(); ++i) {
        const auto& obs = bundle.observations[i];
        const std::string opath = "observation[" + obs.id.value + "]";
        append_report(report, validate_parameter_observation(obs));

        if (!obs.id.empty() && !obs_index.emplace(obs.id, &obs).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                "observations[" + std::to_string(i) + "].id",
                "duplicate ObservationId in bundle");
        }
        if (obs.component_id != bundle.component.id) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                opath + ".component_id",
                "observation component_id does not match bundle component.id");
        }

        for (std::size_t j = 0; j < obs.evidence_refs.size(); ++j) {
            const auto ev_it = ev_index.find(obs.evidence_refs[j]);
            if (ev_it == ev_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    opath + ".evidence_refs[" + std::to_string(j) + "]",
                    "observation references unknown EvidenceId");
            } else {
                const auto doc_it = doc_index.find(ev_it->second->source_document_id);
                if (doc_it != doc_index.end() &&
                    doc_it->second->is_synthetic &&
                    !bundle.component.is_synthetic) {
                    add_issue(
                        report,
                        ValidationRuleCode::synthetic_provenance_mismatch,
                        opath + ".evidence_refs[" + std::to_string(j) + "]",
                        "non-synthetic component cannot draw observations from synthetic SourceDocument");
                }
            }
        }

        for (std::size_t j = 0; j < obs.review_history.size(); ++j) {
            const auto& dec = obs.review_history[j];
            if (!dec.id.empty() && !all_decision_ids.insert(dec.id).second) {
                add_issue(
                    report,
                    ValidationRuleCode::duplicate_identifier,
                    opath + ".review_history[" + std::to_string(j) + "].id",
                    "ReviewDecisionId must be unique across bundle");
            }
            for (std::size_t k = 0; k < dec.evidence_refs.size(); ++k) {
                if (ev_index.find(dec.evidence_refs[k]) == ev_index.end()) {
                    add_issue(
                        report,
                        ValidationRuleCode::unresolved_reference,
                        opath + ".review_history[" + std::to_string(j) + "].evidence_refs[" +
                            std::to_string(k) + "]",
                        "review decision references unknown EvidenceId");
                }
            }
        }
    }

    // Verify supersession targets exist, belong to the same component, and do not form cycles.
    for (const auto& obs : bundle.observations) {
        if (!obs.superseded_by.has_value()) {
            continue;
        }
        const auto target_it = obs_index.find(*obs.superseded_by);
        if (target_it == obs_index.end()) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                "observation[" + obs.id.value + "].superseded_by",
                "superseded_by references unknown ObservationId");
            continue;
        }
        std::set<ObservationId> visited{obs.id};
        const ParameterObservation* cursor = target_it->second;
        while (cursor != nullptr) {
            if (!visited.insert(cursor->id).second) {
                add_issue(
                    report,
                    ValidationRuleCode::invalid_superseding_target,
                    "observation[" + obs.id.value + "].superseded_by",
                    "cyclic supersession chain detected");
                break;
            }
            if (!cursor->superseded_by.has_value()) {
                break;
            }
            const auto next_it = obs_index.find(*cursor->superseded_by);
            cursor = (next_it != obs_index.end()) ? next_it->second : nullptr;
        }
    }

    std::map<NormalizedParameterId, const NormalizedParameter*> norm_index;
    if (bundle.knowledge.has_value()) {
        const auto& kn = *bundle.knowledge;
        const std::string kbase = "component_knowledge[" + kn.id.value + "]";
        append_report(report, validate_component_knowledge(kn));

        if (!kn.review_decision.id.empty() &&
            !all_decision_ids.insert(kn.review_decision.id).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                kbase + ".review_decision.id",
                "ComponentKnowledge ReviewDecisionId duplicates an observation ReviewDecisionId");
        }
        if (kn.component != bundle.component) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                kbase + ".component",
                "ComponentKnowledge embedded ComponentIdentity must match bundle component exactly");
        }
        for (std::size_t i = 0; i < kn.review_decision.evidence_refs.size(); ++i) {
            if (ev_index.find(kn.review_decision.evidence_refs[i]) == ev_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    kbase + ".review_decision.evidence_refs[" + std::to_string(i) + "]",
                    "ComponentKnowledge review_decision references unknown EvidenceId");
            }
        }

        for (std::size_t i = 0; i < kn.approved_observation_refs.size(); ++i) {
            const auto& oid = kn.approved_observation_refs[i];
            const auto it = obs_index.find(oid);
            if (it == obs_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    kbase + ".approved_observation_refs[" + std::to_string(i) + "]",
                    "approved_observation_refs contains unknown ObservationId");
            } else if (it->second->review_state != ReviewState::approved) {
                add_issue(
                    report,
                    ValidationRuleCode::unapproved_observation_in_knowledge,
                    kbase + ".approved_observation_refs[" + std::to_string(i) + "]",
                    "only ReviewState::approved observations may appear in approved_observation_refs");
            }
        }

        for (std::size_t i = 0; i < kn.conflicts.size(); ++i) {
            const auto& conflict = kn.conflicts[i];
            const std::string cpath = kbase + ".conflicts[" + std::to_string(i) + "]";
            for (std::size_t j = 0; j < conflict.competing_observations.size(); ++j) {
                if (obs_index.find(conflict.competing_observations[j]) == obs_index.end()) {
                    add_issue(
                        report,
                        ValidationRuleCode::unresolved_reference,
                        cpath + ".competing_observations[" + std::to_string(j) + "]",
                        "ParameterConflict references unknown ObservationId");
                }
            }
            if (conflict.status == ConflictResolutionStatus::resolved_by_review &&
                conflict.selected_observation_id.has_value()) {
                const auto sel_it = obs_index.find(*conflict.selected_observation_id);
                if (sel_it != obs_index.end() &&
                    sel_it->second->review_state != ReviewState::approved) {
                    add_issue(
                        report,
                        ValidationRuleCode::unapproved_observation_in_knowledge,
                        cpath + ".selected_observation_id",
                        "resolved conflict selected_observation_id must be in ReviewState::approved");
                }
            }
        }

        for (std::size_t i = 0; i < kn.normalized_parameters.size(); ++i) {
            const auto& param = kn.normalized_parameters[i];
            const std::string ppath = kbase + ".normalized_parameters[" + std::to_string(i) + "]";
            if (!param.id.empty()) {
                norm_index.emplace(param.id, &param);
            }
            const auto obs_it = obs_index.find(param.source_observation_id);
            if (obs_it == obs_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    ppath + ".source_observation_id",
                    "NormalizedParameter references unknown ObservationId");
                continue;
            }
            const ParameterObservation& src = *obs_it->second;
            if (src.review_state != ReviewState::approved) {
                add_issue(
                    report,
                    ValidationRuleCode::unapproved_observation_in_knowledge,
                    ppath + ".source_observation_id",
                    "NormalizedParameter cannot reference a non-approved observation");
            }
            if (param.qualifier != src.qualifier) {
                add_issue(
                    report,
                    ValidationRuleCode::qualifier_mismatch,
                    ppath + ".qualifier",
                    "NormalizedParameter qualifier must match source observation qualifier");
            }
            if (param.category != src.category) {
                add_issue(
                    report,
                    ValidationRuleCode::category_mismatch,
                    ppath + ".category",
                    "NormalizedParameter category must match source observation category");
            }
            if (!src.original_value_text.has_value() ||
                param.preserved_original_value_text != *src.original_value_text ||
                param.preserved_original_unit_symbol != src.original_unit_symbol) {
                add_issue(
                    report,
                    ValidationRuleCode::original_notation_mismatch,
                    ppath + ".preserved_original_value_text",
                    "NormalizedParameter must preserve original value text and unit symbol verbatim");
            }
        }

        // Detect competing approved observations with identical name/category/qualifier/conditions
        // and disagreeing values that were silently omitted from kn.conflicts.
        using ObsSlotKey = std::tuple<std::string, ParameterCategory, ValueQualifier, ConditionKey>;
        std::map<ObsSlotKey, std::vector<const ParameterObservation*>> approved_obs_groups;
        for (const auto& obs : bundle.observations) {
            if (obs.review_state == ReviewState::approved) {
                const ObsSlotKey okey{
                    obs.parameter_name,
                    obs.category,
                    obs.qualifier,
                    canonical_condition_key(obs.operating_conditions)};
                approved_obs_groups[okey].push_back(&obs);
            }
        }
        for (const auto& [okey, group] : approved_obs_groups) {
            if (group.size() < 2U) {
                continue;
            }
            bool disagrees = false;
            for (std::size_t i = 1; i < group.size(); ++i) {
                if (group[i]->original_value_text != group[0]->original_value_text ||
                    group[i]->original_unit_symbol != group[0]->original_unit_symbol ||
                    group[i]->parsed_numeric_value != group[0]->parsed_numeric_value) {
                    disagrees = true;
                    break;
                }
            }
            if (!disagrees) {
                continue;
            }
            const bool documented = std::any_of(
                kn.conflicts.begin(),
                kn.conflicts.end(),
                [&](const ParameterConflict& c) {
                    return std::all_of(group.begin(), group.end(), [&](const auto* gobs) {
                        return std::find(
                                   c.competing_observations.begin(),
                                   c.competing_observations.end(),
                                   gobs->id) != c.competing_observations.end();
                    });
                });
            if (!documented) {
                add_issue(
                    report,
                    ValidationRuleCode::undeclared_parameter_conflict,
                    kbase + ".conflicts",
                    "competing approved observations with disagreeing values must be recorded in conflicts");
            }
        }
    }

    std::map<ModelValidationId, const ModelValidationRecord*> val_index;
    for (std::size_t i = 0; i < bundle.model_validations.size(); ++i) {
        const auto& rec = bundle.model_validations[i];
        append_report(report, validate_model_validation_record(rec));
        if (!rec.id.empty() && !val_index.emplace(rec.id, &rec).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                "model_validations[" + std::to_string(i) + "].id",
                "duplicate ModelValidationId in bundle");
        }
    }

    std::map<ModelArtifactId, const ModelArtifactDescriptor*> model_index;
    for (std::size_t i = 0; i < bundle.models.size(); ++i) {
        const auto& model = bundle.models[i];
        const std::string mbase = "model_artifact[" + model.id.value + "]";
        append_report(report, validate_model_artifact_descriptor(model));

        if (!model.id.empty() && !model_index.emplace(model.id, &model).second) {
            add_issue(
                report,
                ValidationRuleCode::duplicate_identifier,
                "models[" + std::to_string(i) + "].id",
                "duplicate ModelArtifactId in bundle");
        }
        if (model.component_id != bundle.component.id) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                mbase + ".component_id",
                "ModelArtifactDescriptor component_id does not match bundle component.id");
        }
        if (bundle.component.is_synthetic && !model.is_synthetic) {
            add_issue(
                report,
                ValidationRuleCode::synthetic_provenance_mismatch,
                mbase + ".is_synthetic",
                "model artifact for a synthetic component must be marked is_synthetic");
        }
        if (model.component_knowledge_id.has_value()) {
            if (!bundle.knowledge.has_value() ||
                *model.component_knowledge_id != bundle.knowledge->id) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    mbase + ".component_knowledge_id",
                    "ModelArtifactDescriptor references unknown ComponentKnowledgeId");
            }
        }
        for (std::size_t j = 0; j < model.parameter_mappings.size(); ++j) {
            const auto& pm = model.parameter_mappings[j];
            if (!pm.knowledge_param_id.has_value()) {
                continue;
            }
            const auto nit = norm_index.find(*pm.knowledge_param_id);
            if (nit == norm_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    mbase + ".parameter_mappings[" + std::to_string(j) + "].knowledge_param_id",
                    "parameter mapping references unknown NormalizedParameterId");
            } else {
                if (nit->second->category == ParameterCategory::absolute_maximum_rating) {
                    add_issue(
                        report,
                        ValidationRuleCode::absolute_maximum_misused_as_model_parameter,
                        mbase + ".parameter_mappings[" + std::to_string(j) + "].knowledge_param_id",
                        "absolute_maximum_rating cannot be bound as a behavioral model parameter");
                }
                if (!are_parameter_units_compatible(pm.expected_unit, *nit->second, pm.mapping_expression)) {
                    const std::string resolved_unit_str = nit->second->unitless
                        ? std::string("unitless")
                        : nit->second->normalized_si_unit.value_or(std::string("<none>"));
                    add_issue(
                        report,
                        ValidationRuleCode::incompatible_parameter_units,
                        mbase + ".parameter_mappings[" + std::to_string(j) + "].expected_unit",
                        "expected_unit '" + pm.expected_unit + "' is incompatible with normalized parameter unit '" +
                            resolved_unit_str + "'");
                }
            }
        }
        for (std::size_t j = 0; j < model.validation_record_refs.size(); ++j) {
            const auto vit = val_index.find(model.validation_record_refs[j]);
            if (vit == val_index.end() || vit->second->model_id != model.id) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    mbase + ".validation_record_refs[" + std::to_string(j) + "]",
                    "model references unknown or mismatched ModelValidationRecord");
            }
        }
    }

    for (const auto& rec : bundle.model_validations) {
        const std::string vbase = "model_validation[" + rec.id.value + "]";
        const auto mit = model_index.find(rec.model_id);
        if (mit == model_index.end()) {
            add_issue(
                report,
                ValidationRuleCode::unresolved_reference,
                vbase + ".model_id",
                "ModelValidationRecord references unknown ModelArtifactId");
        } else {
            const auto& supported = mit->second->supported_analyses;
            for (std::size_t j = 0; j < rec.verified_analyses.size(); ++j) {
                if (std::find(supported.begin(), supported.end(), rec.verified_analyses[j]) ==
                    supported.end()) {
                    add_issue(
                        report,
                        ValidationRuleCode::invalid_model_validation_record,
                        vbase + ".verified_analyses[" + std::to_string(j) + "]",
                        "verified_analyses contains an AnalysisType not declared in model supported_analyses");
                }
            }
        }
        for (std::size_t j = 0; j < rec.evidence_refs.size(); ++j) {
            if (ev_index.find(rec.evidence_refs[j]) == ev_index.end()) {
                add_issue(
                    report,
                    ValidationRuleCode::unresolved_reference,
                    vbase + ".evidence_refs[" + std::to_string(j) + "]",
                    "ModelValidationRecord references unknown EvidenceId");
            }
        }
    }

    return report;
}

void ensure_valid(const ValidationReport& report) {
    if (report.valid()) {
        return;
    }
    const auto& first = report.issues.front();
    throw std::invalid_argument(
        "domain validation failed at " + first.entity_path + ": " + first.message +
        " (" + std::to_string(report.issues.size()) + " issue(s))");
}

} // namespace datasim::component
