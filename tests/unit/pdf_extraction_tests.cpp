#include "datasim/pdf/table_extraction.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef DATASIM_SYNTHETIC_PDF_PATH
#error DATASIM_SYNTHETIC_PDF_PATH must be defined
#endif
#ifndef DATASIM_IMAGE_ONLY_PDF_PATH
#error DATASIM_IMAGE_ONLY_PDF_PATH must be defined
#endif
#ifndef DATASIM_HEADER_BINDING_PDF_PATH
#error DATASIM_HEADER_BINDING_PDF_PATH must be defined
#endif

namespace {
int checks = 0;
void require(bool condition, const char* expression) {
    ++checks;
    if (!condition) throw std::runtime_error(std::string("failed check: ") + expression);
}

std::filesystem::path locate(const char* name) {
    const char* environment = std::getenv("PATH");
    if (environment == nullptr) return {};
    const std::string paths(environment);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(';', start);
        auto candidate = std::filesystem::path(paths.substr(start, end - start)) / name;
        if (std::filesystem::exists(candidate)) return candidate;
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return {};
}

datasim::pdf::ExtractionRequest request_for(std::size_t page,
                                             const std::string& family,
                                             const std::string& mpn,
                                             const std::string& document_id) {
    using namespace datasim;
    pdf::ExtractionRequest request;
    request.pdf_path = DATASIM_SYNTHETIC_PDF_PATH;
    request.pdfinfo_path = locate("pdfinfo.exe");
    request.pdftotext_path = locate("pdftotext.exe");
    request.pdf_page_index = page;
    request.printed_page_label = page == 0 ? "5" : "Appendix C-2";
    request.curator = {"fixture-curator", component::ActorKind::human_engineer, std::nullopt};
    request.component = {{"cmp.fixture." + family}, "Datasim Synthetic", mpn, std::nullopt,
                         "fixture-package", family == "resistor" ? component::ComponentFamily::resistor
                                                                    : component::ComponentFamily::capacitor,
                         std::nullopt, true};
    request.source_document = {{document_id}, "Datasim Synthetic", "Fictional electrical characteristics",
                               {mpn}, "fixture-rev-1", std::nullopt, std::nullopt, std::nullopt,
                               {std::nullopt, false, "Generated fictional PDF for automated testing."}, true};
    request.table.table_id = family == "resistor" ? "TABLE_R" : "TABLE_C";
    request.table.region_x_min = 20; request.table.region_y_min = 80;
    request.table.region_x_max = 690; request.table.region_y_max = 215;
    request.table.row_y_tolerance = 2.0;
    request.table.header_y_min = 80; request.table.header_y_max = 110;
    request.table.parameter_column = {20, 215};
    request.table.unit_column = {215, 285};
    request.table.minimum_column = {285, 350};
    request.table.typical_column = {350, 420};
    request.table.maximum_column = {420, 490};
    request.table.condition_column = pdf::HorizontalSpan{490, 690};
    request.table.category = component::ParameterCategory::electrical_characteristic;
    return request;
}
}

int main() {
    try {
        using namespace datasim;
        auto resistor_request = request_for(0, "resistor", "SIM-R-10K", "doc.synthetic.rc");
        require(!resistor_request.pdfinfo_path.empty(), "pdfinfo executable found on PATH");
        require(!resistor_request.pdftotext_path.empty(), "pdftotext executable found on PATH");
        const auto first = pdf::extract_electrical_table(resistor_request);
        require(first.candidate_bundle.has_value(), "resistor table yields validated candidate bundle");
        require(first.manifest.has_value(), "intake manifest retained");
        require(first.manifest->declared_pdf_page_count == 2U, "actual PDF page count read");
        require(first.manifest->observations.size() == 9U, "three resistor rows have separate min typ max observations");
        require(first.manifest->observations.front().observation.original_unit_symbol == "kohm",
                "original unit spelling preserved");
        require(first.manifest->observations.front().observation.original_value_text == "9.9",
                "original value text preserved");
        require(first.manifest->observations.front().observation.parsed_numeric_value == 9.9,
                "unambiguous scalar parsed in original unit");
        require(first.manifest->observations.front().observation.operating_conditions.size() == 2U,
                "multiple explicitly formatted operating conditions parsed");
        require(first.manifest->observations.front().observation.operating_conditions.front().original_value_text == "25",
                "condition value remains source text");
        require(first.manifest->observations[3].observation.operating_conditions.front().original_value_text == "85",
                "different operating conditions remain attached to separate rows");
        require(first.manifest->observations.front().raw_conditions_text == "T_A=25 degC; V_DD=5 V",
                "original condition cell text remains in the intake sidecar");
        require(first.manifest->observations.front().observation.evidence_refs.size() == 2U,
                "observation links to parameter and value source cells");
        require(first.manifest->evidence.front().location.pdf_page_index == 0U,
                "PDF index is zero-based");
        require(first.manifest->evidence.front().location.printed_page_label == "5",
                "printed page label is manually supplied and distinct");
        require(first.candidate_bundle->observations.front().review_state == component::ReviewState::candidate,
                "extraction result is candidate only");
        require(first.candidate_bundle->knowledge == std::nullopt && first.candidate_bundle->models.empty(),
                "no knowledge or models created");
        require(first.report.has_code(intake::IntakeDiagnosticCode::conflict_candidate),
                "conflicting repeated observations remain visible with diagnostics");

        const auto repeated = pdf::extract_electrical_table(resistor_request);
        require(repeated.candidate_bundle == first.candidate_bundle,
                "repeat extraction produces exactly equivalent candidate records and order");
        bool same_manifest = repeated.manifest.has_value() && first.manifest.has_value() &&
            repeated.manifest->component == first.manifest->component &&
            repeated.manifest->source_document == first.manifest->source_document &&
            repeated.manifest->declared_pdf_page_count == first.manifest->declared_pdf_page_count &&
            repeated.manifest->evidence.size() == first.manifest->evidence.size() &&
            repeated.manifest->observations.size() == first.manifest->observations.size();
        if (same_manifest) {
            for (std::size_t i = 0; i < first.manifest->evidence.size(); ++i) {
                same_manifest = same_manifest &&
                    repeated.manifest->evidence[i].location == first.manifest->evidence[i].location &&
                    repeated.manifest->evidence[i].row_label == first.manifest->evidence[i].row_label &&
                    repeated.manifest->evidence[i].column_label == first.manifest->evidence[i].column_label;
            }
            for (std::size_t i = 0; i < first.manifest->observations.size(); ++i) {
                same_manifest = same_manifest &&
                    repeated.manifest->observations[i].observation == first.manifest->observations[i].observation &&
                    repeated.manifest->observations[i].raw_conditions_text == first.manifest->observations[i].raw_conditions_text;
            }
        }
        require(same_manifest, "repeat extraction produces exactly equivalent provenance");
        require(repeated.diagnostics == first.diagnostics,
                "repeat extraction produces equivalent diagnostics");
        require(repeated.report.diagnostics == first.report.diagnostics &&
                    repeated.report.domain_validation == first.report.domain_validation,
                "repeat extraction produces equivalent intake and domain validation diagnostics");

        auto capacitor_request = request_for(1, "capacitor", "SIM-C-1UF", "doc.synthetic.cap");
        const auto capacitor = pdf::extract_electrical_table(capacitor_request);
        require(capacitor.candidate_bundle.has_value(), "capacitor table yields candidate bundle");
        require(capacitor.manifest->observations.size() == 11U,
                "missing cells stay absent while separate qualifier cells remain");
        require(capacitor.manifest->observations.front().observation.original_value_text == "1,000" ||
                capacitor.manifest->observations[1].observation.original_value_text == "1,000",
                "ambiguous numeric string remains verbatim");
        bool ambiguous_retained = false;
        bool footnote_retained = false;
        for (const auto& entry : capacitor.manifest->observations) {
            if (entry.observation.original_value_text == "1,000")
                ambiguous_retained = !entry.observation.parsed_numeric_value.has_value();
            if (entry.observation.original_value_text == "1.0*")
                footnote_retained = !entry.observation.parsed_numeric_value.has_value() &&
                                    entry.observation.original_unit_symbol == "uF";
        }
        require(ambiguous_retained, "ambiguous number is not coerced");
        require(footnote_retained, "footnote-marked value remains unparsed with original unit");
        bool footnote_reference_retained = false;
        for (const auto& evidence : capacitor.manifest->evidence)
            if (!evidence.location.footnote_refs.empty()) footnote_reference_retained = true;
        require(footnote_reference_retained, "footnote marker remains linked in evidence provenance");
        require(!capacitor.manifest->observations[8].observation.original_unit_symbol.has_value() &&
                    !capacitor.manifest->observations[8].observation.unitless &&
                    !capacitor.manifest->observations[8].observation.parsed_numeric_value.has_value(),
                "blank unit remains unknown and is not inferred as unitless");
        require(capacitor.manifest->evidence.front().location.printed_page_label == "Appendix C-2",
                "non-numeric printed label is preserved explicitly");

        auto wrong_mpn = request_for(0, "resistor", "SIM-R-WRONG", "doc.synthetic.bad-mpn");
        wrong_mpn.source_document.associated_part_numbers = {"SIM-R-10K"};
        const auto bad_part = pdf::extract_electrical_table(wrong_mpn);
        require(!bad_part.candidate_bundle.has_value(), "incorrect exact part number rejected by intake boundary");

        auto out_of_range = request_for(2, "resistor", "SIM-R-10K", "doc.synthetic.range");
        require(!pdf::extract_electrical_table(out_of_range).candidate_bundle.has_value(),
                "out-of-range PDF page rejected");

        auto unsupported = request_for(0, "resistor", "SIM-R-10K", "doc.synthetic.unsupported");
        unsupported.table.region_x_min = 1; unsupported.table.region_x_max = 10;
        require(!pdf::extract_electrical_table(unsupported).candidate_bundle.has_value(),
                "table region without selectable words rejected");

        auto overlapping = request_for(0, "resistor", "SIM-R-10K", "doc.synthetic.overlap");
        overlapping.table.unit_column = {200, 285};
        const auto overlap_result = pdf::extract_electrical_table(overlapping);
        require(!overlap_result.candidate_bundle.has_value(), "overlapping column layout rejected");
        require(!overlap_result.diagnostics.empty() &&
                overlap_result.diagnostics.front().find("overlap") != std::string::npos,
                "unsupported column layout reports why it failed");

        auto image_only = request_for(0, "resistor", "SIM-R-10K", "doc.synthetic.image-only");
        image_only.pdf_path = DATASIM_IMAGE_ONLY_PDF_PATH;
        const auto image_result = pdf::extract_electrical_table(image_only);
        require(!image_result.candidate_bundle.has_value(), "image-only page never creates observations");
        require(!image_result.diagnostics.empty() &&
                image_result.diagnostics.front().find("no selectable text") != std::string::npos,
                "image-only PDF fails closed with selectable-text diagnostic");

        // Phase 4F synthetic regression tests (Cases A-H):
        // Case A (unit only in column header), Case B (explicit row unit consistent with header),
        // Case E (valid condition unit in header + bare numeric condition),
        // Case F (non-numeric condition code "A" remains unresolved with raw_conditions_text),
        // Case G (two value columns typ=uF and max=uA bound independently).
        auto hb_req = request_for(0, "capacitor", "SIM-HB-CAP", "doc.synthetic.hb");
        hb_req.pdf_path = DATASIM_HEADER_BINDING_PDF_PATH;
        hb_req.printed_page_label = "HB-1";
        hb_req.table.table_id = "TABLE_HB_BAND1";
        hb_req.table.region_x_min = 20; hb_req.table.region_y_min = 85;
        hb_req.table.region_x_max = 650; hb_req.table.region_y_max = 165;
        hb_req.table.header_y_min = 85; hb_req.table.header_y_max = 110;
        hb_req.table.parameter_column = {20, 215};
        hb_req.table.unit_column = {215, 280};
        hb_req.table.minimum_column = {280, 340};
        hb_req.table.typical_column = {340, 425};
        hb_req.table.maximum_column = {425, 505};
        hb_req.table.condition_column = pdf::HorizontalSpan{505, 650};

        // Case H check on synthetic_header_binding_datasheet.pdf without header bindings:
        const auto hb_unbound = pdf::extract_electrical_table(hb_req);
        require(hb_unbound.candidate_bundle.has_value(), "Case H: unbound extraction yields candidate bundle");
        require(hb_unbound.manifest->observations.size() == 3U, "Case H: 3 observations extracted without bindings");
        require(!hb_unbound.manifest->observations[0].observation.original_unit_symbol.has_value() &&
                !hb_unbound.manifest->observations[0].observation.parsed_numeric_value.has_value(),
                "Case H: blank row unit without header binding leaves unit and scalar empty");
        require(hb_unbound.manifest->observations[0].observation.operating_conditions.empty() &&
                hb_unbound.manifest->observations[0].raw_conditions_text == "100",
                "Case H: bare condition without header binding remains unresolved with raw text preserved");

        // Configure explicit header unit bindings (Cases A, B, G) and header condition binding (Cases E, F):
        hb_req.table.header_unit_bindings = {
            {component::ValueQualifier::typical, {340, 425}, 90, 105, "Capacitance (uF)", "uF"},
            {component::ValueQualifier::maximum, {425, 505}, 90, 105, "Leakage (uA)", "uA"},
        };
        hb_req.table.header_condition_binding = pdf::HeaderConditionBinding{
            {510, 585}, 90, 105, "VOLTAGE (V)", "VOLTAGE", "V"};

        const auto hb_bound = pdf::extract_electrical_table(hb_req);
        require(hb_bound.candidate_bundle.has_value(), "Cases A/B/E/F/G: header-bound extraction yields valid candidate bundle");
        require(hb_bound.manifest->observations.size() == 3U, "Cases A/B/E/F/G: 3 observations extracted");
        for (const auto& obs : hb_bound.candidate_bundle->observations) {
            require(obs.review_state == component::ReviewState::candidate && obs.review_history.empty(),
                    "Cases A/B/E/F/G: all extracted observations remain candidate-only");
        }
        // Row 1 typ (Case A + Case E + Case G col 1):
        const auto& obs_r1_typ = hb_bound.manifest->observations[0].observation;
        require(obs_r1_typ.parameter_name == "CAPACITANCE_A", "Case A: parameter name preserved");
        require(obs_r1_typ.original_unit_symbol == "uF", "Case A: typ unit bound from header");
        require(obs_r1_typ.parsed_numeric_value == 0.22, "Case A: typ scalar parsed after header unit binding");
        require(obs_r1_typ.operating_conditions.size() == 1U &&
                obs_r1_typ.operating_conditions[0].parameter_name == "VOLTAGE" &&
                obs_r1_typ.operating_conditions[0].original_value_text == "100" &&
                obs_r1_typ.operating_conditions[0].unit_symbol == "V" &&
                obs_r1_typ.operating_conditions[0].numeric_value == 100.0,
                "Case E: bare numeric condition cell parsed using bound condition header");
        require(obs_r1_typ.evidence_refs.size() == 4U,
                "Cases A+E: observation links to parameter, value, header_unit, and header_condition evidence");

        // Row 1 max (Case G col 2 + Case E):
        const auto& obs_r1_max = hb_bound.manifest->observations[1].observation;
        require(obs_r1_max.original_unit_symbol == "uA" && obs_r1_max.parsed_numeric_value == 0.5,
                "Case G: second value column independently binds its own header unit (uA)");
        require(obs_r1_max.evidence_refs.size() == 4U,
                "Case G: max observation links to its own header_unit.max evidence");

        // Row 2 typ (Case B + Case F):
        const auto& entry_r2_typ = hb_bound.manifest->observations[2];
        const auto& obs_r2_typ = entry_r2_typ.observation;
        require(obs_r2_typ.parameter_name == "CAPACITANCE_B", "Case B: row 2 parameter preserved");
        require(obs_r2_typ.original_unit_symbol == "uF" && obs_r2_typ.parsed_numeric_value == 0.33,
                "Case B: explicit row unit consistent with header unit succeeds");
        require(obs_r2_typ.evidence_refs.size() == 2U,
                "Case B + Case F: explicit row unit takes precedence and unresolved code 'A' does not attach header evidence");
        require(obs_r2_typ.operating_conditions.empty() && entry_r2_typ.raw_conditions_text == "A" &&
                entry_r2_typ.ambiguity_notes.has_value(),
                "Case F: non-numeric condition code 'A' remains unresolved with raw_conditions_text preserved");
        require(hb_bound.diagnostics.size() == 1U &&
                hb_bound.diagnostics.front().find("operating condition left unresolved: A") != std::string::npos,
                "Case F: unresolved condition diagnostic emitted for non-numeric code 'A'");

        // Case C — Row/header unit conflict (Band 2 on Page 1: row unit "nF" vs header unit "uF"):
        auto hb_conflict = hb_req;
        hb_conflict.table.table_id = "TABLE_HB_BAND2";
        hb_conflict.table.region_y_min = 285; hb_conflict.table.region_y_max = 340;
        hb_conflict.table.header_y_min = 285; hb_conflict.table.header_y_max = 310;
        hb_conflict.table.header_unit_bindings = {
            {component::ValueQualifier::typical, {340, 425}, 290, 305, "Capacitance (uF)", "uF"}};
        hb_conflict.table.header_condition_binding = std::nullopt;
        const auto conflict_res = pdf::extract_electrical_table(hb_conflict);
        require(!conflict_res.candidate_bundle.has_value(), "Case C: row/header unit conflict fails closed");
        require(!conflict_res.diagnostics.empty() &&
                conflict_res.diagnostics.front().find("conflicts with bound header unit") != std::string::npos,
                "Case C: conflict diagnostic reports explicit row unit vs bound header unit");

        // Case D — Missing or mismatched header fails closed:
        auto hb_mismatch = hb_req;
        hb_mismatch.table.header_unit_bindings = {
            {component::ValueQualifier::typical, {340, 425}, 90, 105, "Capacitance (pF)", "pF"}};
        const auto mismatch_res = pdf::extract_electrical_table(hb_mismatch);
        require(!mismatch_res.candidate_bundle.has_value(), "Case D1: mismatched header text fails closed");
        require(!mismatch_res.diagnostics.empty() &&
                mismatch_res.diagnostics.front().find("header binding text mismatch") != std::string::npos,
                "Case D1: reports header binding text mismatch");

        auto hb_empty_box = hb_req;
        hb_empty_box.table.header_unit_bindings = {
            {component::ValueQualifier::minimum, {305, 338}, 90, 105, "Min (uF)", "uF"}};
        const auto empty_box_res = pdf::extract_electrical_table(hb_empty_box);
        require(!empty_box_res.candidate_bundle.has_value(), "Case D2: empty header binding box fails closed");
        require(!empty_box_res.diagnostics.empty() &&
                empty_box_res.diagnostics.front().find("contains no selectable text") != std::string::npos,
                "Case D2: reports empty header binding box");

        auto hb_straddle = hb_req;
        hb_straddle.table.header_unit_bindings = {
            {component::ValueQualifier::typical, {360, 425}, 90, 105, "Capacitance (uF)", "uF"}};
        const auto straddle_res = pdf::extract_electrical_table(hb_straddle);
        require(!straddle_res.candidate_bundle.has_value(), "Case D3: straddling header word fails closed");
        require(!straddle_res.diagnostics.empty() &&
                straddle_res.diagnostics.front().find("straddles configured header binding box") != std::string::npos,
                "Case D3: reports straddling header word");

        // Unicode preservation check (Page 2 of synthetic_header_binding_datasheet.pdf has "(µF)" = \xC2\xB5F):
        auto hb_unicode = hb_req;
        hb_unicode.pdf_page_index = 1;
        hb_unicode.printed_page_label = "HB-2";
        hb_unicode.table.table_id = "TABLE_HB_UNICODE";
        hb_unicode.table.typical_column = {340, 445};
        hb_unicode.table.maximum_column = {445, 505};
        hb_unicode.table.header_unit_bindings = {
            {component::ValueQualifier::typical, {340, 440}, 90, 105, "CAPACITANCE (\xC2\xB5" "F)", "\xC2\xB5" "F"}};
        hb_unicode.table.header_condition_binding = std::nullopt;
        const auto unicode_res = pdf::extract_electrical_table(hb_unicode);
        require(unicode_res.candidate_bundle.has_value(), "Unicode header unit binding succeeds");
        require(unicode_res.manifest->observations.size() == 1U &&
                unicode_res.manifest->observations.front().observation.original_unit_symbol == std::string("\xC2\xB5" "F") &&
                unicode_res.manifest->observations.front().observation.parsed_numeric_value == 0.22,
                "Unicode unit symbol preserved verbatim without ASCII conversion");

        std::cout << "PDF extraction checks: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDF extraction failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
