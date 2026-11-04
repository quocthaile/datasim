#include "intake_fixtures.hpp"

namespace datasim::intake::testing {

IntakeManifest make_synthetic_resistor_manifest() {
    IntakeManifest manifest;
    manifest.origin = IntakeOriginClass::synthetic_fixture;
    manifest.curator = {"curator.synthetic", component::ActorKind::human_engineer, "Synthetic curator"};
    manifest.component.id = {"comp.intake.synthetic.resistor"};
    manifest.component.manufacturer = "Datasim Synthetic Passive Corp";
    manifest.component.part_number = "SYN-INTAKE-1K-LF";
    manifest.component.part_number_suffix = "LF";
    manifest.component.package_id = "0603";
    manifest.component.family = component::ComponentFamily::resistor;
    manifest.component.is_synthetic = true;

    manifest.source_document.id = {"doc.intake.synthetic.resistor"};
    manifest.source_document.publisher = manifest.component.manufacturer;
    manifest.source_document.title = "Synthetic resistor fixture";
    manifest.source_document.associated_part_numbers = {manifest.component.part_number};
    manifest.source_document.revision = "SYN-A";
    manifest.source_document.original_url = "urn:datasim:synthetic:intake:resistor";
    manifest.source_document.retrieval_date = "2026-11-04";
    manifest.source_document.content_sha256 =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    manifest.source_document.usage_rights = {
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic fixture; no vendor material is included."};
    manifest.source_document.is_synthetic = true;
    manifest.declared_pdf_page_count = 4;

    component::EvidenceLocation evidence;
    evidence.id = {"ev.intake.synthetic.resistor"};
    evidence.source_document_id = manifest.source_document.id;
    evidence.pdf_page_index = 2;
    evidence.printed_page_label = "1";
    evidence.section = "Electrical characteristics";
    evidence.table_or_figure_id = "Table 1";
    evidence.cell_or_region = "R_DC row";
    manifest.evidence.push_back({evidence, "R_DC", "Typical", false});

    auto operating_evidence = evidence;
    operating_evidence.id = {"ev.intake.synthetic.resistor.operating"};
    operating_evidence.section = "Recommended operating conditions";
    operating_evidence.cell_or_region = "T_A row";
    manifest.evidence.push_back({operating_evidence, "T_A", "Recommended", false});

    auto limits_evidence = evidence;
    limits_evidence.id = {"ev.intake.synthetic.resistor.limits"};
    limits_evidence.section = "Absolute maximum ratings";
    limits_evidence.cell_or_region = "P_MAX row";
    manifest.evidence.push_back({limits_evidence, "P_MAX", "Maximum", false});

    component::ParameterObservation observation;
    observation.id = {"obs.intake.synthetic.resistance.typ"};
    observation.component_id = manifest.component.id;
    observation.parameter_name = "R_DC";
    observation.original_value_text = "1.000";
    observation.original_unit_symbol = "kohm";
    observation.parsed_numeric_value = 1.0;
    observation.qualifier = component::ValueQualifier::typical;
    observation.category = component::ParameterCategory::electrical_characteristic;
    observation.operating_conditions.push_back({"T_A", "25", "degC", 25.0, std::nullopt});
    observation.evidence_refs = {evidence.id};
    observation.extraction_method = component::ExtractionMethod::synthetic_fixture;
    observation.extraction_tool = "datasim-intake-fixture";
    observation.extraction_tool_version = "1.0.0";
    observation.extracted_at = "2026-11-04T10:00:00Z";
    manifest.observations.push_back({observation, "R_DC", "typical", "T_A = 25 degC", std::nullopt});

    auto minimum = observation;
    minimum.id = {"obs.intake.synthetic.resistance.min"};
    minimum.original_value_text = "0.990";
    minimum.parsed_numeric_value = 0.990;
    minimum.qualifier = component::ValueQualifier::minimum;
    manifest.observations.push_back({minimum, "R_DC", "minimum", "T_A = 25 degC", std::nullopt});

    auto maximum = observation;
    maximum.id = {"obs.intake.synthetic.resistance.max"};
    maximum.original_value_text = "1.010";
    maximum.parsed_numeric_value = 1.010;
    maximum.qualifier = component::ValueQualifier::maximum;
    manifest.observations.push_back({maximum, "R_DC", "maximum", "T_A = 25 degC", std::nullopt});

    component::ParameterObservation recommended;
    recommended.id = {"obs.intake.synthetic.temperature.recommended"};
    recommended.component_id = manifest.component.id;
    recommended.parameter_name = "T_A";
    recommended.original_value_text = "25";
    recommended.original_unit_symbol = "degC";
    recommended.parsed_numeric_value = 25.0;
    recommended.qualifier = component::ValueQualifier::typical;
    recommended.category = component::ParameterCategory::recommended_operating_condition;
    recommended.evidence_refs = {operating_evidence.id};
    recommended.extraction_method = component::ExtractionMethod::synthetic_fixture;
    recommended.extraction_tool = "datasim-intake-fixture";
    recommended.extraction_tool_version = "1.0.0";
    recommended.extracted_at = "2026-11-04T10:00:00Z";
    manifest.observations.push_back({recommended, "T_A", "typical", std::nullopt, std::nullopt});

    component::ParameterObservation absolute_maximum = recommended;
    absolute_maximum.id = {"obs.intake.synthetic.power.absolute_maximum"};
    absolute_maximum.parameter_name = "P_MAX";
    absolute_maximum.original_value_text = "0.1";
    absolute_maximum.original_unit_symbol = "W";
    absolute_maximum.parsed_numeric_value = 0.1;
    absolute_maximum.qualifier = component::ValueQualifier::maximum;
    absolute_maximum.category = component::ParameterCategory::absolute_maximum_rating;
    absolute_maximum.evidence_refs = {limits_evidence.id};
    manifest.observations.push_back(
        {absolute_maximum, "P_MAX", "maximum", std::nullopt, std::nullopt});
    return manifest;
}

IntakeManifest make_synthetic_capacitor_manifest() {
    auto manifest = make_synthetic_resistor_manifest();
    manifest.component.id = {"comp.intake.synthetic.capacitor"};
    manifest.component.part_number = "SYN-INTAKE-1U-C0G";
    manifest.component.part_number_suffix = "C0G";
    manifest.component.family = component::ComponentFamily::capacitor;
    manifest.source_document.id = {"doc.intake.synthetic.capacitor"};
    manifest.source_document.associated_part_numbers = {manifest.component.part_number};
    manifest.source_document.title = "Synthetic capacitor fixture";
    manifest.source_document.original_url = "urn:datasim:synthetic:intake:capacitor";
    for (auto& entry : manifest.evidence) {
        entry.location.source_document_id = manifest.source_document.id;
    }
    for (auto& entry : manifest.observations) {
        entry.observation.component_id = manifest.component.id;
    }
    manifest.observations.resize(1);
    auto& observation = manifest.observations.front().observation;
    observation.id = {"obs.intake.synthetic.capacitance.typ"};
    observation.parameter_name = "C";
    observation.original_value_text = "1.000";
    observation.original_unit_symbol = "uF";
    observation.parsed_numeric_value = 1.0;
    observation.qualifier = component::ValueQualifier::typical;
    observation.category = component::ParameterCategory::electrical_characteristic;
    observation.operating_conditions = {{"T_A", "25", "degC", 25.0, std::nullopt}};
    observation.evidence_refs = {manifest.evidence.front().location.id};
    return manifest;
}

} // namespace datasim::intake::testing
