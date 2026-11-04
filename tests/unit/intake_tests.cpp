#include "intake_fixtures.hpp"
#include "datasim/intake/numeric/raw_numeric.hpp"
#include <cassert>
#include <cmath>

int main() {
    using namespace datasim::intake;
    using namespace datasim::intake::testing;

    assert(parse_unambiguous_scalar("1").value() == 1.0);
    assert(parse_unambiguous_scalar("1e-6").value() == 1.0e-6);
    assert(!parse_unambiguous_scalar("1,000").has_value());
    assert(!parse_unambiguous_scalar("1-5").has_value());
    assert(!parse_unambiguous_scalar("nan").has_value());

    auto manifest = make_synthetic_resistor_manifest();
    const auto report = validate_intake_manifest(manifest);
    assert(report.valid());
    const auto built = build_candidate_bundle(manifest);
    assert(built.bundle.has_value());
    assert(built.bundle->observations.front().review_state ==
           datasim::component::ReviewState::candidate);
    assert(built.bundle->observations.front().review_history.empty());
    assert(built.bundle->observations.front().original_unit_symbol == "kohm");
    assert(built.bundle->observations.front().parsed_numeric_value == 1.0);
    assert(validate_intake_manifest(make_synthetic_capacitor_manifest()).valid());

    manifest.observations.front().observation.original_value_text = "1,000";
    manifest.observations.front().observation.parsed_numeric_value.reset();
    const auto ambiguous = validate_intake_manifest(manifest);
    assert(ambiguous.valid());
    assert(ambiguous.has_code(IntakeDiagnosticCode::ambiguous_numeric_text));
    assert(build_candidate_bundle(manifest).bundle.has_value());

    manifest.observations.front().observation.parsed_numeric_value = 1000.0;
    const auto asserted_ambiguous = validate_intake_manifest(manifest);
    assert(!asserted_ambiguous.valid());
    assert(asserted_ambiguous.has_code(IntakeDiagnosticCode::asserted_ambiguous_numeric_value));

    manifest = make_synthetic_resistor_manifest();
    manifest.evidence.front().location.pdf_page_index = 4;
    const auto out_of_range = validate_intake_manifest(manifest);
    assert(!out_of_range.valid());
    assert(out_of_range.has_code(IntakeDiagnosticCode::page_index_out_of_range));

    manifest = make_synthetic_resistor_manifest();
    manifest.observations.front().observation.review_state =
        datasim::component::ReviewState::approved;
    const auto non_candidate = build_candidate_bundle(manifest);
    assert(!non_candidate.bundle.has_value());
    assert(non_candidate.report.has_code(IntakeDiagnosticCode::non_candidate_state_in_intake));

    manifest = make_synthetic_resistor_manifest();
    manifest.source_document.publisher = "Other Synthetic Publisher";
    const auto mismatch = validate_intake_manifest(manifest);
    assert(!mismatch.valid());
    assert(mismatch.has_code(IntakeDiagnosticCode::manufacturer_publisher_mismatch));
    return 0;
}
