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

        std::cout << "PDF extraction checks: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "PDF extraction failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
