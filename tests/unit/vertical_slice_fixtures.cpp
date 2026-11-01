#include "vertical_slice_fixtures.hpp"
#include "component_fixtures.hpp"

namespace datasim::vertical::testing {

CircuitAssembly make_validated_synthetic_rc_assembly() {
    auto res_bundle = component::testing::make_synthetic_resistor_bundle();
    auto cap_bundle = component::testing::make_synthetic_capacitor_bundle();

    const component::ReviewerIdentity model_reviewer{
        "eng.synthetic.model-validator-1",
        component::ActorKind::human_engineer,
        "Synthetic Simulation Model Qualification Engineer"};

    // Add validated_within_envelope record for the resistor analytical_equation model.
    component::ModelValidationRecord res_val;
    res_val.id = component::ModelValidationId{"mval.synthetic.resistor.validated.v1"};
    res_val.model_id = res_bundle.models.front().id;
    res_val.model_version = res_bundle.models.front().version;
    res_val.status = component::ModelValidationStatus::validated_within_envelope;
    res_val.validation_kind = component::ModelValidationKind::analytical_comparison;
    res_val.reviewer = model_reviewer;
    res_val.timestamp = "2026-11-01T08:00:00Z";
    res_val.summary = "Validated synthetic ideal ohmic resistor model against closed-form V = R * I within envelope.";
    res_val.evidence_refs = {component::EvidenceId{"ev.res.table3.electrical"}};
    res_val.verified_analyses = {
        component::AnalysisType::dc_operating_point,
        component::AnalysisType::transient};
    res_bundle.model_validations.push_back(res_val);
    res_bundle.models.front().validation_record_refs.push_back(res_val.id);

    // Add an ideal analytical_equation capacitor model alongside the existing spice_subcircuit model.
    component::ModelArtifactDescriptor cap_ideal_model;
    cap_ideal_model.id = component::ModelArtifactId{"model.synthetic.capacitor.ideal-linear"};
    cap_ideal_model.component_id = cap_bundle.component.id;
    cap_ideal_model.component_knowledge_id = cap_bundle.knowledge->id;
    cap_ideal_model.version = "1.0.0";
    cap_ideal_model.format = component::ModelFormat::analytical_equation;
    cap_ideal_model.source_vendor = "Datasim Synthetic Model Catalog";
    cap_ideal_model.artifact_uri = "urn:datasim:synthetic:equation:ideal-linear-capacitor:1.0.0";
    cap_ideal_model.artifact_sha256 = std::nullopt;
    cap_ideal_model.license = component::ModelLicenseMetadata{
        "CC0-1.0-Synthetic-Test-Fixture",
        true,
        "Synthetic analytical capacitor model metadata for vertical-slice testing."};
    cap_ideal_model.terminals = {
        component::TerminalDescriptor{1U, "port_a", "1", "First symmetric capacitor electrode"},
        component::TerminalDescriptor{2U, "port_b", "2", "Second symmetric capacitor electrode"}};
    cap_ideal_model.parameter_mappings = {
        component::ModelParameterMapping{
            "C",
            component::NormalizedParameterId{"np.cap.c.typ"},
            std::nullopt,
            "F",
            "I(port_a,port_b) = C * dV(port_a,port_b)/dt"}};
    cap_ideal_model.validity_envelope = component::ModelValidityEnvelope{
        {component::NumericRangeConstraint{"ambient_temperature", -55.0, 125.0, "degC", true, true},
         component::NumericRangeConstraint{"terminal_dc_voltage", -50.0, 50.0, "V", true, true},
         component::NumericRangeConstraint{"signal_frequency", 0.0, 1.0e7, "Hz", true, true}},
        {"Ideal linear time-invariant capacitance within rated 50 V DC envelope."},
        "Excludes ESR, ESL, and dielectric absorption."};
    cap_ideal_model.supported_analyses = {
        component::AnalysisType::dc_operating_point,
        component::AnalysisType::ac_small_signal,
        component::AnalysisType::transient};
    cap_ideal_model.limitations = {
        "Does not model equivalent series resistance (ESR) or inductance (ESL).",
        "Does not model dielectric breakdown above 50 V DC."};

    component::ModelValidationRecord cap_val;
    cap_val.id = component::ModelValidationId{"mval.synthetic.capacitor.validated.v1"};
    cap_val.model_id = cap_ideal_model.id;
    cap_val.model_version = cap_ideal_model.version;
    cap_val.status = component::ModelValidationStatus::validated_within_envelope;
    cap_val.validation_kind = component::ModelValidationKind::analytical_comparison;
    cap_val.reviewer = model_reviewer;
    cap_val.timestamp = "2026-11-01T08:05:00Z";
    cap_val.summary = "Validated synthetic ideal linear capacitor model against closed-form RC transient step response.";
    cap_val.evidence_refs = {component::EvidenceId{"ev.cap.table2.characteristics"}};
    cap_val.verified_analyses = {
        component::AnalysisType::dc_operating_point,
        component::AnalysisType::transient};

    cap_ideal_model.validation_record_refs = {cap_val.id};
    cap_ideal_model.is_synthetic = true;
    cap_bundle.models.push_back(cap_ideal_model);
    cap_bundle.model_validations.push_back(cap_val);

    CircuitComponentInstance r1;
    r1.instance_id = ComponentInstanceId{"R1"};
    r1.role = CircuitElementRole::series_resistor;
    r1.component_id = res_bundle.component.id;
    r1.knowledge_id = res_bundle.knowledge->id;
    r1.expected_knowledge_version = res_bundle.knowledge->version;
    r1.selected_model_id = res_bundle.models.front().id;
    r1.selected_model_version = res_bundle.models.front().version;
    r1.terminal_bindings = {
        TerminalNodeBinding{1U, "p1", "1", "vin"},
        TerminalNodeBinding{2U, "p2", "2", "vout"}};

    CircuitComponentInstance c1;
    c1.instance_id = ComponentInstanceId{"C1"};
    c1.role = CircuitElementRole::shunt_capacitor;
    c1.component_id = cap_bundle.component.id;
    c1.knowledge_id = cap_bundle.knowledge->id;
    c1.expected_knowledge_version = cap_bundle.knowledge->version;
    c1.selected_model_id = cap_ideal_model.id;
    c1.selected_model_version = cap_ideal_model.version;
    c1.terminal_bindings = {
        TerminalNodeBinding{1U, "port_a", "1", "vout"},
        TerminalNodeBinding{2U, "port_b", "2", "0"}};

    CircuitAssembly assembly;
    assembly.id = CircuitAssemblyId{"circuit.synthetic.rc-lowpass-v1"};
    assembly.name = "Synthetic First-Order RC Low-Pass Step-Response Circuit";
    assembly.topology = CircuitTopologyKind::series_rc_lowpass;
    assembly.input_node = "vin";
    assembly.output_node = "vout";
    assembly.ground_node = "0";
    assembly.component_bundles = {std::move(res_bundle), std::move(cap_bundle)};
    assembly.instances = {std::move(r1), std::move(c1)};
    return assembly;
}

CircuitAssembly make_engineering_approved_rc_assembly() {
    auto assembly = make_validated_synthetic_rc_assembly();
    assembly.id = CircuitAssemblyId{"circuit.eng.rc-lowpass-v1"};
    assembly.name = "Engineering-Reviewed First-Order RC Reference Circuit";

    for (auto& bundle : assembly.component_bundles) {
        bundle.component.is_synthetic = false;
        if (bundle.knowledge.has_value()) {
            bundle.knowledge->component.is_synthetic = false;
        }
        for (auto& doc : bundle.documents) {
            doc.is_synthetic = false;
        }
        for (auto& obs : bundle.observations) {
            obs.extraction_method = component::ExtractionMethod::manual_entry;
            obs.extraction_tool = std::nullopt;
            obs.extraction_tool_version = std::nullopt;
        }
        for (auto& model : bundle.models) {
            model.is_synthetic = false;
        }
    }
    return assembly;
}

RcRunInputSpecification make_synthetic_test_run_inputs() {
    RcRunInputSpecification inputs;
    inputs.requested_analysis = component::AnalysisType::transient;
    inputs.execution_policy = ExecutionPolicy::test_only_synthetic;
    return inputs;
}

} // namespace datasim::vertical::testing
