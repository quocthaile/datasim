#include "intake_fixtures.hpp"
#include "datasim/intake/numeric/raw_numeric.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

int checks = 0;

void require(bool condition, const char* expression) {
    ++checks;
    if (!condition) {
        throw std::runtime_error(std::string("failed check: ") + expression);
    }
}

} // namespace

int main() {
    try {
        using namespace datasim::intake;
        using namespace datasim::intake::testing;

        require(parse_unambiguous_scalar("1").value() == 1.0, "integer scalar");
        require(parse_unambiguous_scalar("1e-6").value() == 1.0e-6, "scientific notation");
        require(parse_unambiguous_scalar("-0.5").value() == -0.5, "negative scalar");
        require(parse_unambiguous_scalar("0").value() == 0.0, "zero scalar");
        require(!parse_unambiguous_scalar("1,000").has_value(), "locale separator rejected");
        require(!parse_unambiguous_scalar("1-5").has_value(), "range rejected");
        require(!parse_unambiguous_scalar("<0.1").has_value(), "inequality rejected");
        require(!parse_unambiguous_scalar("±5%").has_value(), "tolerance rejected");
        require(!parse_unambiguous_scalar(" 1.0").has_value(), "leading whitespace rejected");
        require(!parse_unambiguous_scalar("1.0 ").has_value(), "trailing whitespace rejected");
        require(!parse_unambiguous_scalar("nan").has_value(), "NaN rejected");
        require(!parse_unambiguous_scalar("inf").has_value(), "infinity rejected");

        auto manifest = make_synthetic_resistor_manifest();
        require(validate_intake_manifest(manifest).valid(), "valid resistor manifest");
        auto built = build_candidate_bundle(manifest);
        require(built.bundle.has_value(), "candidate bundle exists");
        require(built.bundle->observations.front().review_state ==
                    datasim::component::ReviewState::candidate,
                "candidate state preserved");
        require(built.bundle->observations.front().review_history.empty(),
                "review history remains empty");
        require(built.bundle->observations.front().original_unit_symbol == "kohm",
                "original unit preserved");
        require(built.bundle->observations.front().parsed_numeric_value == 1.0,
                "original-unit parsed value preserved");
        require(validate_intake_manifest(make_synthetic_capacitor_manifest()).valid(),
                "valid capacitor manifest");

        manifest.observations.front().observation.original_value_text = "1.000";
        manifest.observations.front().observation.parsed_numeric_value = 2.0;
        auto mismatch = validate_intake_manifest(manifest);
        require(!mismatch.valid(), "raw/parsed mismatch rejected");
        require(mismatch.has_code(IntakeDiagnosticCode::raw_numeric_mismatch),
                "raw/parsed mismatch diagnostic");

        manifest.observations.front().observation.original_value_text = "1,000";
        manifest.observations.front().observation.parsed_numeric_value.reset();
        auto ambiguous = validate_intake_manifest(manifest);
        require(ambiguous.valid(), "ambiguous candidate remains recoverable");
        require(ambiguous.has_code(IntakeDiagnosticCode::ambiguous_numeric_text),
                "ambiguous warning emitted");
        require(build_candidate_bundle(manifest).bundle.has_value(),
                "ambiguous candidate bundle retained");

        manifest.observations.front().observation.parsed_numeric_value = 1000.0;
        auto asserted_ambiguous = validate_intake_manifest(manifest);
        require(!asserted_ambiguous.valid(), "asserted ambiguous value rejected");
        require(asserted_ambiguous.has_code(
                    IntakeDiagnosticCode::asserted_ambiguous_numeric_value),
                "asserted ambiguity diagnostic");

        manifest = make_synthetic_resistor_manifest();
        manifest.observations.front().observation.original_value_text = "1.000";
        manifest.observations.front().observation.parsed_numeric_value.reset();
        auto omitted_parse = validate_intake_manifest(manifest);
        require(omitted_parse.valid(), "missing parsed scalar remains candidate");

        manifest = make_synthetic_resistor_manifest();
        manifest.evidence.front().location.pdf_page_index = 4;
        auto out_of_range = validate_intake_manifest(manifest);
        require(!out_of_range.valid(), "page bound rejected");
        require(out_of_range.has_code(IntakeDiagnosticCode::page_index_out_of_range),
                "page bound diagnostic");

        manifest = make_synthetic_resistor_manifest();
        manifest.observations.front().observation.review_state =
            datasim::component::ReviewState::approved;
        auto non_candidate = build_candidate_bundle(manifest);
        require(!non_candidate.bundle.has_value(), "non-candidate bundle rejected");
        require(non_candidate.report.has_code(
                    IntakeDiagnosticCode::non_candidate_state_in_intake),
                "non-candidate diagnostic");

        manifest = make_synthetic_resistor_manifest();
        manifest.component.part_number = "RC0603FR-071KL/NOPB";
        manifest.component.part_number_suffix = "FR-071KL/NOPB";
        manifest.source_document.associated_part_numbers = {manifest.component.part_number};
        require(validate_intake_manifest(manifest).valid(),
                "manufacturer-specific suffix without separator assumption");
        manifest.component.part_number_suffix.reset();
        require(validate_intake_manifest(manifest).valid(), "missing suffix accepted");

        manifest = make_synthetic_resistor_manifest();
        manifest.observations[2].observation.operating_conditions.front().original_value_text =
            "85";
        manifest.observations[2].observation.operating_conditions.front().numeric_value = 85.0;
        manifest.observations[2].raw_conditions_text = "T_A = 85 degC";
        require(validate_intake_manifest(manifest).valid(),
                "bounds at different temperatures are not compared");
        manifest.observations[2].observation.original_unit_symbol = "ohm";
        require(validate_intake_manifest(manifest).valid(),
                "bounds with different units are not compared");

        manifest = make_synthetic_resistor_manifest();
        auto duplicate_value = manifest.observations.front();
        duplicate_value.observation.id = {"obs.intake.synthetic.resistance.conflict"};
        duplicate_value.observation.original_value_text = "1.005";
        duplicate_value.observation.parsed_numeric_value = 1.005;
        manifest.observations.push_back(duplicate_value);
        auto conflict = validate_intake_manifest(manifest);
        require(conflict.valid(), "conflicting candidates remain recoverable");
        require(conflict.has_code(IntakeDiagnosticCode::conflict_candidate),
                "conflicting candidates are diagnosed");
        require(build_candidate_bundle(manifest).bundle.has_value(),
                "conflicting candidates remain visible in bundle");

        manifest = make_synthetic_resistor_manifest();
        manifest.source_document.publisher = "Other Synthetic Publisher";
        auto publisher_mismatch = validate_intake_manifest(manifest);
        require(!publisher_mismatch.valid(), "publisher mismatch rejected");
        require(publisher_mismatch.has_code(
                    IntakeDiagnosticCode::manufacturer_publisher_mismatch),
                "publisher mismatch diagnostic");
        manifest.publisher_difference_declaration = "Curated document publisher differs.";
        require(validate_intake_manifest(manifest).valid(),
                "explicit publisher difference accepted");

        manifest = make_synthetic_resistor_manifest();
        manifest.evidence.push_back(manifest.evidence.front());
        auto duplicate_evidence = validate_intake_manifest(manifest);
        require(!duplicate_evidence.valid(), "duplicate evidence rejected");
        require(duplicate_evidence.domain_validation.has_code(
                    datasim::component::ValidationRuleCode::duplicate_identifier),
                "duplicate evidence diagnostic");

        manifest = make_synthetic_resistor_manifest();
        manifest.observations.front().observation.evidence_refs = {
            datasim::component::EvidenceId{"ev.unknown"}};
        auto unknown_evidence = validate_intake_manifest(manifest);
        require(!unknown_evidence.valid(), "unknown evidence rejected");
        require(unknown_evidence.domain_validation.has_code(
                    datasim::component::ValidationRuleCode::unresolved_reference),
                "unknown evidence diagnostic");

        manifest = make_synthetic_resistor_manifest();
        manifest.component.part_number_suffix = "WRONG";
        auto suffix_mismatch = validate_intake_manifest(manifest);
        require(!suffix_mismatch.valid(), "invalid suffix rejected");
        require(suffix_mismatch.has_code(
                    IntakeDiagnosticCode::part_number_suffix_mismatch),
                "invalid suffix diagnostic");

        std::cout << "intake checks: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "intake test failure after " << checks << " checks: "
                  << error.what() << '\n';
        return 1;
    }
}
