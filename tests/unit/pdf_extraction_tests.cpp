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
#ifndef DATASIM_INLINE_UNIT_PDF_PATH
#error DATASIM_INLINE_UNIT_PDF_PATH must be defined
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

        // Phase 4G synthetic regression tests (Cases 1-14):
        auto iu_req = request_for(0, "capacitor", "SIM-IU-MIXED", "doc.synthetic.iu");
        iu_req.pdf_path = DATASIM_INLINE_UNIT_PDF_PATH;
        iu_req.printed_page_label = "IU-1";
        iu_req.table.table_id = "TABLE_INLINE_BAND1";
        iu_req.table.region_x_min = 20; iu_req.table.region_y_min = 60;
        iu_req.table.region_x_max = 650; iu_req.table.region_y_max = 260;
        iu_req.table.header_y_min = 60; iu_req.table.header_y_max = 85;
        iu_req.table.parameter_column = {20, 215};
        iu_req.table.unit_column = {215, 265};
        iu_req.table.minimum_column = {265, 340};
        iu_req.table.typical_column = {340, 420};
        iu_req.table.maximum_column = {420, 505};
        iu_req.table.condition_column = pdf::HorizontalSpan{505, 650};

        // Case 14 — Legacy Phase 4F behavior with inline parsing disabled:
        const auto iu_disabled = pdf::extract_electrical_table(iu_req);
        require(iu_disabled.candidate_bundle.has_value(), "Case 14: legacy extraction with inline parsing disabled succeeds");
        require(iu_disabled.manifest->observations.size() == 10U, "Case 14: 10 cells extracted across 7 rows");
        require(iu_disabled.manifest->observations[0].observation.original_value_text == "200 V" &&
                !iu_disabled.manifest->observations[0].observation.original_unit_symbol.has_value() &&
                !iu_disabled.manifest->observations[0].observation.parsed_numeric_value.has_value(),
                "Case 14: without inline binding, '200 V' remains unsplit with blank unit and no scalar");
        require(iu_disabled.value_cell_provenance.size() == 10U &&
                iu_disabled.value_cell_provenance[0].raw_cell_text == "200 V" &&
                iu_disabled.value_cell_provenance[0].raw_value_token == "200 V" &&
                !iu_disabled.value_cell_provenance[0].raw_inline_unit_token.has_value(),
                "Case 14: provenance records unsplit raw cell text when inline parsing is disabled");

        // Configure explicit inline value/unit bindings for min, typ, max (Cases 1, 2, 3, 4, 7, 8, 10, 11):
        iu_req.table.inline_value_unit_bindings = {
            {component::ValueQualifier::minimum, {"pF", "V"}},
            {component::ValueQualifier::typical, {"pF", "nF"}},
            {component::ValueQualifier::maximum, {"V", "W"}},
        };
        const auto iu_enabled = pdf::extract_electrical_table(iu_req);
        require(iu_enabled.candidate_bundle.has_value(), "Cases 1-4/7/8/10/11: inline-enabled extraction yields valid candidate bundle");
        require(iu_enabled.manifest->observations.size() == 10U, "Cases 1-4/7/8/10/11: 10 observations extracted");
        require(iu_enabled.value_cell_provenance.size() == 10U, "Cases 1-4/7/8/10/11: 10 cell provenance records retained");
        for (const auto& obs : iu_enabled.candidate_bundle->observations) {
            require(obs.review_state == component::ReviewState::candidate && obs.review_history.empty(),
                    "Phase 4G: all inline-parsed observations remain candidate-only");
        }

        // Case 1 — "200 V" (Row 1 max):
        const auto& c1_obs = iu_enabled.manifest->observations[0].observation;
        const auto& c1_prov = iu_enabled.value_cell_provenance[0];
        require(c1_obs.id.value == "obs.doc.synthetic.iu.TABLE_INLINE_BAND1.p0.r1.max",
                "Case 1: deterministic observation ID preserved");
        require(c1_obs.parameter_name == "Operating voltage" &&
                c1_obs.original_value_text == "200" &&
                c1_obs.original_unit_symbol == "V" &&
                c1_obs.parsed_numeric_value == 200.0,
                "Case 1: '200 V' split into value '200', unit 'V', scalar 200.0");
        require(c1_prov.observation_id == c1_obs.id &&
                c1_prov.raw_cell_text == "200 V" &&
                c1_prov.raw_value_token == "200" &&
                c1_prov.raw_inline_unit_token == "V",
                "Case 1: full source cell '200 V' preserved in ValueCellProvenance");

        // Case 2 — "0.4 W" (Row 2 max):
        const auto& c2_obs = iu_enabled.manifest->observations[1].observation;
        const auto& c2_prov = iu_enabled.value_cell_provenance[1];
        require(c2_obs.parameter_name == "Rated dissipation" &&
                c2_obs.original_value_text == "0.4" &&
                c2_obs.original_unit_symbol == "W" &&
                c2_obs.parsed_numeric_value == 0.4 &&
                c2_prov.raw_cell_text == "0.4 W",
                "Case 2: '0.4 W' split into decimal '0.4', unit 'W', scalar 0.4, raw cell '0.4 W'");

        // Case 3 & Case 11 — "1.0 pF" (Row 3 min) and "220 pF" (Row 3 typ) in adjacent columns [265,340] and [340,420]:
        const auto& c3_min_obs = iu_enabled.manifest->observations[2].observation;
        const auto& c3_typ_obs = iu_enabled.manifest->observations[3].observation;
        require(c3_min_obs.qualifier == component::ValueQualifier::minimum &&
                c3_min_obs.original_value_text == "1.0" &&
                c3_min_obs.original_unit_symbol == "pF" &&
                c3_min_obs.parsed_numeric_value == 1.0 &&
                iu_enabled.value_cell_provenance[2].raw_cell_text == "1.0 pF",
                "Case 11: adjacent minimum column '1.0 pF' parsed without cross-column leakage");
        require(c3_typ_obs.qualifier == component::ValueQualifier::typical &&
                c3_typ_obs.original_value_text == "220" &&
                c3_typ_obs.original_unit_symbol == "pF" &&
                c3_typ_obs.parsed_numeric_value == 220.0 &&
                iu_enabled.value_cell_provenance[3].raw_cell_text == "220 pF",
                "Case 3 & Case 11: '220 pF' prefixed unit parsed in adjacent typical column");

        // Case 4 — "56 pF" (Row 4 min) and "10 nF" (Row 4 typ, second prefixed unit in same column):
        const auto& c4_typ_obs = iu_enabled.manifest->observations[5].observation;
        require(c4_typ_obs.parameter_name == "Capacitance X7R" &&
                c4_typ_obs.original_value_text == "10" &&
                c4_typ_obs.original_unit_symbol == "nF" &&
                c4_typ_obs.parsed_numeric_value == 10.0 &&
                iu_enabled.value_cell_provenance[5].raw_cell_text == "10 nF",
                "Case 4: '10 nF' prefixed unit parsed in same column as '220 pF' without unit conversion");

        // Case 7 — Missing unit (Row 5 max: bare "200" with blank row unit and no header binding):
        const auto& c7_obs = iu_enabled.manifest->observations[6].observation;
        const auto& c7_prov = iu_enabled.value_cell_provenance[6];
        require(c7_obs.parameter_name == "Unbound rating" &&
                c7_obs.original_value_text == "200" &&
                !c7_obs.original_unit_symbol.has_value() &&
                !c7_obs.unitless &&
                !c7_obs.parsed_numeric_value.has_value() &&
                c7_prov.raw_cell_text == "200" &&
                !c7_prov.raw_inline_unit_token.has_value(),
                "Case 7: bare '200' without unit leaves unit and scalar unasserted");

        // Case 8 — Ambiguous number ("1,000 V" on Row 6 max):
        const auto& c8_obs = iu_enabled.manifest->observations[7].observation;
        const auto& c8_prov = iu_enabled.value_cell_provenance[7];
        require(c8_obs.parameter_name == "Isolation voltage" &&
                c8_obs.original_value_text == "1,000" &&
                c8_obs.original_unit_symbol == "V" &&
                !c8_obs.parsed_numeric_value.has_value() &&
                c8_prov.raw_cell_text == "1,000 V" &&
                c8_prov.raw_value_token == "1,000" &&
                c8_prov.raw_inline_unit_token == "V",
                "Case 8: '1,000 V' preserves ambiguous number '1,000', unit 'V', raw cell '1,000 V', and no scalar");
        require(iu_enabled.report.has_code(intake::IntakeDiagnosticCode::ambiguous_numeric_text),
                "Case 8: intake report flags ambiguous_numeric_text warning for '1,000'");

        // Case 10 — Footnote-marked source text ("200* V" in Row 7 min and "200 V*" in Row 7 max):
        const auto& c10_min_obs = iu_enabled.manifest->observations[8].observation;
        const auto& c10_max_entry = iu_enabled.manifest->observations[9];
        const auto& c10_max_obs = c10_max_entry.observation;
        require(c10_min_obs.original_value_text == "200*" &&
                c10_min_obs.original_unit_symbol == "V" &&
                !c10_min_obs.parsed_numeric_value.has_value() &&
                iu_enabled.value_cell_provenance[8].raw_cell_text == "200* V",
                "Case 10a: '200* V' preserves '200*' and unit 'V' without scalar");
        require(c10_max_obs.original_value_text == "200" &&
                c10_max_obs.original_unit_symbol == "V" &&
                !c10_max_obs.parsed_numeric_value.has_value() &&
                c10_max_entry.ambiguity_notes.has_value() &&
                iu_enabled.value_cell_provenance[9].raw_cell_text == "200 V*" &&
                iu_enabled.value_cell_provenance[9].raw_inline_unit_token == "V",
                "Case 10b: '200 V*' strips '*' from unit symbol, preserves '200 V*' in provenance, and leaves scalar unasserted");

        // Repeat-run determinism check for Phase 4G:
        const auto iu_repeated = pdf::extract_electrical_table(iu_req);
        require(iu_repeated.candidate_bundle == iu_enabled.candidate_bundle &&
                iu_repeated.value_cell_provenance == iu_enabled.value_cell_provenance &&
                iu_repeated.diagnostics == iu_enabled.diagnostics,
                "Phase 4G repeat extraction produces identical candidate bundle, provenance, and diagnostics");

        // Case 5 — "200 V" with consistent header binding ("Max (V)" on Row 1 sub-band):
        auto iu_consistent_hdr = iu_req;
        iu_consistent_hdr.table.region_y_max = 110;
        iu_consistent_hdr.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"V"}}};
        iu_consistent_hdr.table.header_unit_bindings = {
            {component::ValueQualifier::maximum, {420, 505}, 65, 80, "Max (V)", "V"}};
        const auto c5_res = pdf::extract_electrical_table(iu_consistent_hdr);
        require(c5_res.candidate_bundle.has_value() &&
                c5_res.manifest->observations.size() == 1U &&
                c5_res.manifest->observations[0].observation.original_value_text == "200" &&
                c5_res.manifest->observations[0].observation.original_unit_symbol == "V" &&
                c5_res.manifest->observations[0].observation.parsed_numeric_value == 200.0 &&
                c5_res.manifest->observations[0].observation.evidence_refs.size() == 2U,
                "Case 5: '200 V' with consistent header binding 'V' succeeds and uses cell-level evidence");

        // Case 6a — "200 V" with conflicting header binding ("Max (mV)" on Band 5):
        auto iu_conflict_hdr = iu_req;
        iu_conflict_hdr.table.table_id = "TABLE_INLINE_BAND5";
        iu_conflict_hdr.table.region_y_min = 535; iu_conflict_hdr.table.region_y_max = 585;
        iu_conflict_hdr.table.header_y_min = 535; iu_conflict_hdr.table.header_y_max = 560;
        iu_conflict_hdr.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"V", "mV"}}};
        iu_conflict_hdr.table.header_unit_bindings = {
            {component::ValueQualifier::maximum, {420, 505}, 540, 555, "Max (mV)", "mV"}};
        const auto c6a_res = pdf::extract_electrical_table(iu_conflict_hdr);
        require(!c6a_res.candidate_bundle.has_value() &&
                !c6a_res.diagnostics.empty() &&
                c6a_res.diagnostics.front().find("inline unit 'V' conflicts with bound header unit 'mV'") != std::string::npos,
                "Case 6a: '200 V' with conflicting header unit 'mV' fails closed without conversion");

        // Case 6b — "200 V" with conflicting explicit row unit ("mV" on Band 3):
        auto iu_conflict_row = iu_req;
        iu_conflict_row.table.table_id = "TABLE_INLINE_BAND3";
        iu_conflict_row.table.region_y_min = 385; iu_conflict_row.table.region_y_max = 435;
        iu_conflict_row.table.header_y_min = 385; iu_conflict_row.table.header_y_max = 410;
        iu_conflict_row.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"V"}}};
        const auto c6b_res = pdf::extract_electrical_table(iu_conflict_row);
        require(!c6b_res.candidate_bundle.has_value() &&
                !c6b_res.diagnostics.empty() &&
                c6b_res.diagnostics.front().find("inline unit 'V' conflicts with explicit row unit 'mV'") != std::string::npos,
                "Case 6b: '200 V' with conflicting explicit row unit 'mV' fails closed");

        // Case 9 — Range ("0.22..0.47 uF" on Band 2) fails closed:
        auto iu_range = iu_req;
        iu_range.table.table_id = "TABLE_INLINE_BAND2";
        iu_range.table.region_y_min = 310; iu_range.table.region_y_max = 360;
        iu_range.table.header_y_min = 310; iu_range.table.header_y_max = 335;
        iu_range.table.inline_value_unit_bindings = {
            {component::ValueQualifier::typical, {"uF"}}};
        const auto c9_res = pdf::extract_electrical_table(iu_range);
        require(!c9_res.candidate_bundle.has_value() &&
                !c9_res.diagnostics.empty() &&
                c9_res.diagnostics.front().find("contains unsupported value representation '0.22..0.47'") != std::string::npos,
                "Case 9: range '0.22..0.47 uF' fails closed and is never coerced to a scalar");

        // Unpermitted inline unit token ("0.4 W" when only {"V"} is allowed) fails closed:
        auto iu_unpermitted_unit = iu_req;
        iu_unpermitted_unit.table.region_y_max = 135;
        iu_unpermitted_unit.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"V"}}};
        const auto unperm_res = pdf::extract_electrical_table(iu_unpermitted_unit);
        require(!unperm_res.candidate_bundle.has_value() &&
                !unperm_res.diagnostics.empty() &&
                unperm_res.diagnostics.front().find("inline unit token 'W' is not permitted") != std::string::npos,
                "Unpermitted inline unit token 'W' fails closed");

        // Case 11 negative check — Straddling inline unit word across narrow column boundary fails closed:
        auto iu_narrow_straddle = iu_req;
        iu_narrow_straddle.table.minimum_column = {265, 298}; // cuts through 'pF' at x=[291.68, 303.35]
        const auto c11_neg = pdf::extract_electrical_table(iu_narrow_straddle);
        require(!c11_neg.candidate_bundle.has_value() &&
                !c11_neg.diagnostics.empty() &&
                c11_neg.diagnostics.front().find("outside or ambiguously across configured column spans") != std::string::npos,
                "Case 11 negative: inline unit word straddling narrow column boundary fails closed");

        // Case 12 — Duplicate or overlapping word boxes inside a single value cell (Band 4) fails closed:
        auto iu_overlap_cell = iu_req;
        iu_overlap_cell.table.table_id = "TABLE_INLINE_BAND4";
        iu_overlap_cell.table.region_y_min = 460; iu_overlap_cell.table.region_y_max = 510;
        iu_overlap_cell.table.header_y_min = 460; iu_overlap_cell.table.header_y_max = 485;
        const auto c12_res = pdf::extract_electrical_table(iu_overlap_cell);
        require(!c12_res.candidate_bundle.has_value() &&
                !c12_res.diagnostics.empty() &&
                c12_res.diagnostics.front().find("overlapping or duplicate word bounding boxes") != std::string::npos,
                "Case 12: overlapping word bounding boxes inside a value cell fail closed");

        // Case 13 — Unicode inline unit preservation (Page 2 of synthetic_inline_unit_datasheet.pdf has "1.0 µF"):
        auto iu_unicode = iu_req;
        iu_unicode.pdf_page_index = 1;
        iu_unicode.printed_page_label = "IU-2";
        iu_unicode.table.table_id = "TABLE_INLINE_UNICODE";
        iu_unicode.table.region_y_min = 85; iu_unicode.table.region_y_max = 135;
        iu_unicode.table.header_y_min = 85; iu_unicode.table.header_y_max = 110;
        iu_unicode.table.inline_value_unit_bindings = {
            {component::ValueQualifier::typical, {std::string("\xC2\xB5" "F")}}};
        const auto c13_res = pdf::extract_electrical_table(iu_unicode);
        require(c13_res.candidate_bundle.has_value() &&
                c13_res.manifest->observations.size() == 1U &&
                c13_res.manifest->observations[0].observation.original_value_text == "1.0" &&
                c13_res.manifest->observations[0].observation.original_unit_symbol == std::string("\xC2\xB5" "F") &&
                c13_res.manifest->observations[0].observation.parsed_numeric_value == 1.0 &&
                c13_res.value_cell_provenance.size() == 1U &&
                c13_res.value_cell_provenance[0].raw_cell_text == std::string("1.0 \xC2\xB5" "F"),
                "Case 13: Unicode inline unit symbol preserved verbatim in observation and cell provenance");

        // Configuration validation negative checks:
        auto iu_empty_allow = iu_req;
        iu_empty_allow.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {}}};
        const auto empty_allow_res = pdf::extract_electrical_table(iu_empty_allow);
        require(!empty_allow_res.candidate_bundle.has_value() &&
                !empty_allow_res.diagnostics.empty() &&
                empty_allow_res.diagnostics.front().find("requires at least one allowed_unit_symbol") != std::string::npos,
                "Empty allowed_unit_symbols in InlineValueUnitBinding rejected");

        auto iu_bad_symbol = iu_req;
        iu_bad_symbol.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"10V"}}};
        const auto bad_symbol_res = pdf::extract_electrical_table(iu_bad_symbol);
        require(!bad_symbol_res.candidate_bundle.has_value() &&
                !bad_symbol_res.diagnostics.empty() &&
                bad_symbol_res.diagnostics.front().find("invalid allowed_unit_symbol") != std::string::npos,
                "Invalid numeric-prefixed allowed_unit_symbol rejected");

        auto iu_dup_qual = iu_req;
        iu_dup_qual.table.inline_value_unit_bindings = {
            {component::ValueQualifier::maximum, {"V"}},
            {component::ValueQualifier::maximum, {"W"}}};
        const auto dup_qual_res = pdf::extract_electrical_table(iu_dup_qual);
        require(!dup_qual_res.candidate_bundle.has_value() &&
                !dup_qual_res.diagnostics.empty() &&
                dup_qual_res.diagnostics.front().find("duplicate inline value/unit binding") != std::string::npos,
                "Duplicate InlineValueUnitBinding for the same qualifier rejected");

        std::cout << "PDF extraction checks: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDF extraction failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
