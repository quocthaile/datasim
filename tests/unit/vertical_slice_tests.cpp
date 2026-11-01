#include "vertical_slice_fixtures.hpp"
#include "component_fixtures.hpp"
#include "datasim/simulation/analysis/grid.hpp"
#include "datasim/simulation/analysis/reference.hpp"
#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/io/report.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include "datasim/vertical/compatibility/model_compatibility.hpp"
#include "datasim/vertical/provenance/run_provenance.hpp"
#include "datasim/vertical/resolution/parameter_resolution.hpp"
#include <cmath>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using namespace datasim;
using namespace datasim::vertical;
using namespace datasim::vertical::testing;

int g_checks = 0;

void require(bool condition, const char* context = "") {
    ++g_checks;
    if (!condition) {
        std::string msg = "vertical slice check failed #" + std::to_string(g_checks);
        if (context != nullptr && context[0] != '\0') {
            msg += " (";
            msg += context;
            msg += ")";
        }
        throw std::runtime_error(msg);
    }
}

void require_throws(const std::function<void()>& fn, const char* context = "") {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    require(threw, context);
}

void test_wp1_circuit_assembly() {
    const auto synth_assembly = make_validated_synthetic_rc_assembly();
    const auto synth_report = validate_circuit_assembly(synth_assembly);
    require(synth_report.valid(), "baseline synthetic RC assembly valid");

    // Verify ComponentDomainBundle single-component responsibility is preserved.
    require(synth_assembly.component_bundles.size() == 2U, "two distinct single-component bundles");
    for (const auto& b : synth_assembly.component_bundles) {
        require(component::validate_domain_bundle(b).valid(), "each bundle independently valid");
    }

    // Verify individual component identities and exact suffixes are preserved.
    const auto* r_bundle = find_bundle_for_component(
        synth_assembly, component::ComponentId{"comp.synthetic.resistor.syn-tf0603-1k00-lf"});
    const auto* c_bundle = find_bundle_for_component(
        synth_assembly, component::ComponentId{"comp.synthetic.capacitor.syn-c0g0805-1u0-j50nt"});
    require(r_bundle != nullptr && c_bundle != nullptr, "bundle lookup by ComponentId");
    require(r_bundle->component.part_number == "SYN-TF0603-1K00-LF", "resistor exact MPN");
    require(r_bundle->component.part_number_suffix == std::optional<std::string>{"LF"}, "resistor suffix");
    require(c_bundle->component.part_number == "SYN-C0G0805-1U0-J50NT", "capacitor exact MPN");
    require(c_bundle->component.part_number_suffix == std::optional<std::string>{"J50NT"}, "capacitor suffix");

    // Verify symmetric 2-terminal node orientation is accepted.
    {
        auto swapped = synth_assembly;
        swapped.instances[0].terminal_bindings[0].circuit_node_id = "vout";
        swapped.instances[0].terminal_bindings[1].circuit_node_id = "vin";
        swapped.instances[1].terminal_bindings[0].circuit_node_id = "0";
        swapped.instances[1].terminal_bindings[1].circuit_node_id = "vout";
        require(validate_circuit_assembly(swapped).valid(), "reversed symmetric 2-terminal passive nodes");
    }

    // Reject empty/invalid assembly ID or name.
    {
        auto bad = synth_assembly;
        bad.id = CircuitAssemblyId{""};
        require(validate_circuit_assembly(bad).has_code(VerticalValidationCode::empty_required_field));
        bad.id = CircuitAssemblyId{"invalid id with spaces"};
        require(validate_circuit_assembly(bad).has_code(VerticalValidationCode::invalid_identifier));
        bad = synth_assembly;
        bad.name = "   ";
        require(validate_circuit_assembly(bad).has_code(VerticalValidationCode::empty_required_field));
    }

    // Reject unsupported topology or non-canonical nodes.
    {
        auto bad = synth_assembly;
        bad.topology = CircuitTopologyKind::unsupported_topology;
        require(validate_circuit_assembly(bad).has_code(VerticalValidationCode::unsupported_circuit_topology));
        bad = synth_assembly;
        bad.input_node = "in_custom";
        require(validate_circuit_assembly(bad).has_code(VerticalValidationCode::invalid_circuit_nodes));
    }

    // Reject duplicate component bundles or duplicate instance IDs.
    {
        auto dup_bundle = synth_assembly;
        dup_bundle.component_bundles.push_back(dup_bundle.component_bundles.front());
        require(validate_circuit_assembly(dup_bundle).has_code(VerticalValidationCode::duplicate_identifier));

        auto dup_inst = synth_assembly;
        dup_inst.instances[1].instance_id = dup_inst.instances[0].instance_id;
        require(validate_circuit_assembly(dup_inst).has_code(VerticalValidationCode::duplicate_identifier));
    }

    // Reject dangling component_id, knowledge_id, knowledge_version, model_id, model_version.
    {
        auto bad_comp = synth_assembly;
        bad_comp.instances[0].component_id = component::ComponentId{"comp.missing.device"};
        require(validate_circuit_assembly(bad_comp).has_code(
            VerticalValidationCode::unresolved_component_reference));

        auto bad_kn = synth_assembly;
        bad_kn.instances[0].knowledge_id = component::ComponentKnowledgeId{"ck.wrong.id"};
        require(validate_circuit_assembly(bad_kn).has_code(
            VerticalValidationCode::unresolved_knowledge_reference));

        auto bad_kn_ver = synth_assembly;
        bad_kn_ver.instances[0].expected_knowledge_version = 99U;
        require(validate_circuit_assembly(bad_kn_ver).has_code(
            VerticalValidationCode::knowledge_version_mismatch));

        auto bad_mod = synth_assembly;
        bad_mod.instances[0].selected_model_id = component::ModelArtifactId{"model.nonexistent"};
        require(validate_circuit_assembly(bad_mod).has_code(
            VerticalValidationCode::unresolved_model_reference));

        auto bad_mod_ver = synth_assembly;
        bad_mod_ver.instances[0].selected_model_version = "9.9.9";
        require(validate_circuit_assembly(bad_mod_ver).has_code(
            VerticalValidationCode::model_version_mismatch));
    }

    // Reject unapproved knowledge or unresolved parameter conflicts in knowledge.
    {
        auto unapproved = synth_assembly;
        unapproved.component_bundles[0].knowledge->review_decision.previous_state =
            component::ReviewState::candidate;
        unapproved.component_bundles[0].knowledge->review_decision.new_state =
            component::ReviewState::needs_review;
        const auto rep = validate_circuit_assembly(unapproved);
        require(rep.has_code(VerticalValidationCode::unapproved_component_knowledge));
        require(rep.has_code(VerticalValidationCode::invalid_component_bundle));

        auto conflicted = synth_assembly;
        conflicted.component_bundles[0].knowledge->conflicts.push_back(component::ParameterConflict{
            "conflict.r_dc.unresolved",
            "R_DC",
            {component::ObservationId{"obs.res.r_dc.typ"},
             component::ObservationId{"obs.res.r_dc.ocr_typo"}},
            component::ConflictResolutionStatus::unresolved,
            std::nullopt,
            "Unresolved resistance discrepancy",
            std::nullopt,
            std::nullopt});
        const auto crep = validate_circuit_assembly(conflicted);
        require(crep.has_code(VerticalValidationCode::unresolved_knowledge_conflicts));
    }

    // Reject wrong component family for role and invalid element role counts.
    {
        auto wrong_family = synth_assembly;
        wrong_family.component_bundles[0].component.family = component::ComponentFamily::inductor;
        wrong_family.component_bundles[0].knowledge->component.family =
            component::ComponentFamily::inductor;
        require(validate_circuit_assembly(wrong_family).has_code(
            VerticalValidationCode::component_family_role_mismatch));

        auto two_resistors = synth_assembly;
        two_resistors.instances[1].role = CircuitElementRole::series_resistor;
        const auto rrep = validate_circuit_assembly(two_resistors);
        require(rrep.has_code(VerticalValidationCode::invalid_topology_element_roles));
        require(rrep.has_code(VerticalValidationCode::component_family_role_mismatch));
    }

    // Reject invalid terminal bindings, pin mismatches, shorted terminals, and miswired nodes.
    {
        auto wrong_pin = synth_assembly;
        wrong_pin.instances[0].terminal_bindings[0].physical_pin_id = "99";
        require(validate_circuit_assembly(wrong_pin).has_code(
            VerticalValidationCode::invalid_terminal_node_binding));

        auto wrong_term_name = synth_assembly;
        wrong_term_name.instances[1].terminal_bindings[0].terminal_name = "wrong_port";
        require(validate_circuit_assembly(wrong_term_name).has_code(
            VerticalValidationCode::invalid_terminal_node_binding));

        auto missing_term = synth_assembly;
        missing_term.instances[0].terminal_bindings.pop_back();
        require(validate_circuit_assembly(missing_term).has_code(
            VerticalValidationCode::invalid_terminal_node_binding));

        auto shorted = synth_assembly;
        shorted.instances[0].terminal_bindings[1].circuit_node_id = "vin";
        require(validate_circuit_assembly(shorted).has_code(
            VerticalValidationCode::shorted_component_terminals));

        auto miswired = synth_assembly;
        miswired.instances[0].terminal_bindings[1].circuit_node_id = "0";
        require(validate_circuit_assembly(miswired).has_code(
            VerticalValidationCode::topology_connectivity_mismatch));
    }
}

void test_wp2_parameter_resolution() {
    const auto assembly = make_validated_synthetic_rc_assembly();
    const auto res = resolve_rc_parameters(assembly);
    require(res.validation.valid(), "baseline parameter resolution valid");
    require(res.parameters.has_value(), "resolved parameters populated");

    const auto& r = res.parameters->resistor;
    const auto& c = res.parameters->capacitor;

    require(r.resolved_si_value == 1000.0, "resolved R = 1000 ohm");
    require(r.normalized_si_unit == "ohm", "resolved R unit = ohm");
    require(r.qualifier == component::ValueQualifier::typical, "resolved R typical qualifier");
    require(r.category == component::ParameterCategory::electrical_characteristic, "resolved R category");
    require(r.normalized_parameter_id == component::NormalizedParameterId{"np.res.r_dc.typ"});
    require(r.source_observation_id == component::ObservationId{"obs.res.r_dc.typ"});
    require(r.preserved_original_value_text == "1.000");
    require(r.preserved_original_unit_symbol == "kohm");
    require(r.conversion_rule == "explicit_scale: 1.000 kohm * 1000 = 1000.0 ohm");
    require(!r.operating_conditions.empty() && r.operating_conditions[0].parameter_name == "T_A");
    require(!r.source_evidence_locations.empty() &&
            r.source_evidence_locations[0].id == component::EvidenceId{"ev.res.table3.electrical"});
    require(!r.source_documents.empty() &&
            r.source_documents[0].id == component::SourceDocumentId{"doc.synthetic.syn-tf0603-spec-rev-a"});

    require(std::abs(c.resolved_si_value - 1.0e-6) < 1e-20, "resolved C = 1e-6 F");
    require(c.normalized_si_unit == "F", "resolved C unit = F");
    require(c.qualifier == component::ValueQualifier::typical, "resolved C typical qualifier");
    require(c.category == component::ParameterCategory::electrical_characteristic, "resolved C category");
    require(c.normalized_parameter_id == component::NormalizedParameterId{"np.cap.c.typ"});
    require(c.source_observation_id == component::ObservationId{"obs.cap.c.typ"});
    require(c.preserved_original_value_text == "1.000");
    require(c.preserved_original_unit_symbol == "uF");
    require(c.conversion_rule == "explicit_scale: 1.000 uF * 1e-6 = 1.0e-6 F");
    require(c.operating_conditions.size() == 3U);
    require(!c.source_evidence_locations.empty() &&
            c.source_evidence_locations[0].id == component::EvidenceId{"ev.cap.table2.characteristics"});

    // Reject missing model parameter mapping or fixed-literal mapping without knowledge_param_id.
    {
        auto no_map = assembly;
        no_map.component_bundles[0].models[0].parameter_mappings.clear();
        require(resolve_rc_parameters(no_map).validation.has_code(
            VerticalValidationCode::missing_model_parameter_mapping));

        auto fixed_only = assembly;
        fixed_only.component_bundles[0].models[0].parameter_mappings[0].knowledge_param_id = std::nullopt;
        fixed_only.component_bundles[0].models[0].parameter_mappings[0].fixed_value = 1000.0;
        require(resolve_rc_parameters(fixed_only).validation.has_code(
            VerticalValidationCode::missing_model_parameter_mapping));
    }

    // Reject extra unsupported model parameter or duplicate conflicting model parameter mappings.
    {
        auto extra_param = assembly;
        extra_param.component_bundles[0].models[0].parameter_mappings.push_back(
            component::ModelParameterMapping{
                "L_PARASITIC", std::nullopt, 1e-9, "H", "Fixed parasitic inductance"});
        require(resolve_rc_parameters(extra_param).validation.has_code(
            VerticalValidationCode::unsupported_model_parameter));

        auto dup_map = assembly;
        dup_map.component_bundles[0].models[0].parameter_mappings.push_back(
            dup_map.component_bundles[0].models[0].parameter_mappings.front());
        require(resolve_rc_parameters(dup_map).validation.has_code(
            VerticalValidationCode::conflicting_model_parameter_mappings));
    }

    // Reject non-typical qualifier (minimum / maximum) and non-electrical-characteristic category.
    {
        auto min_bound = assembly;
        min_bound.component_bundles[0].models[0].parameter_mappings[0].knowledge_param_id =
            component::NormalizedParameterId{"np.res.r_dc.min"};
        require(resolve_rc_parameters(min_bound).validation.has_code(
            VerticalValidationCode::non_typical_parameter_qualifier));

        auto max_bound = assembly;
        max_bound.component_bundles[1].models[1].parameter_mappings[0].knowledge_param_id =
            component::NormalizedParameterId{"np.cap.c.max"};
        require(resolve_rc_parameters(max_bound).validation.has_code(
            VerticalValidationCode::non_typical_parameter_qualifier));

        auto wrong_cat = assembly;
        for (auto& np : wrong_cat.component_bundles[0].knowledge->normalized_parameters) {
            if (np.id == component::NormalizedParameterId{"np.res.r_dc.typ"}) {
                np.category = component::ParameterCategory::recommended_operating_condition;
            }
        }
        require(resolve_rc_parameters(wrong_cat).validation.has_code(
            VerticalValidationCode::invalid_parameter_category));
    }

    // Reject conflicting duplicate typical normalized parameters in knowledge.
    {
        auto dup_typ = assembly;
        auto extra_np = dup_typ.component_bundles[0].knowledge->normalized_parameters[1];
        extra_np.id = component::NormalizedParameterId{"np.res.r_dc.typ.duplicate"};
        extra_np.normalized_numeric_value = 1005.0;
        dup_typ.component_bundles[0].knowledge->normalized_parameters.push_back(extra_np);
        require(resolve_rc_parameters(dup_typ).validation.has_code(
            VerticalValidationCode::conflicting_normalized_parameters));
    }

    // Reject missing, non-SI, or incompatible parameter units and unsupported unit mappings.
    {
        auto non_si = assembly;
        for (auto& np : non_si.component_bundles[0].knowledge->normalized_parameters) {
            if (np.id == component::NormalizedParameterId{"np.res.r_dc.typ"}) {
                np.normalized_si_unit = "kohm";
                np.normalized_numeric_value = 1.0;
            }
        }
        const auto non_si_rep = resolve_rc_parameters(non_si).validation;
        require(non_si_rep.has_code(VerticalValidationCode::non_si_or_incompatible_parameter_unit));

        auto cross_dim = assembly;
        cross_dim.component_bundles[0].models[0].parameter_mappings[0].expected_unit = "F";
        const auto cross_rep = resolve_rc_parameters(cross_dim).validation;
        require(cross_rep.has_code(VerticalValidationCode::non_si_or_incompatible_parameter_unit));
        require(cross_rep.has_code(VerticalValidationCode::unsupported_unit_mapping));

        auto runtime_scale = assembly;
        runtime_scale.component_bundles[0].models[0].parameter_mappings[0].mapping_expression =
            "R = dc_resistance * 1000.0";
        require(resolve_rc_parameters(runtime_scale).validation.has_code(
            VerticalValidationCode::unsupported_unit_mapping));
    }
}

void test_wp3_model_compatibility_and_synthetic_policy() {
    const auto synth_assembly = make_validated_synthetic_rc_assembly();
    const auto resolved = *resolve_rc_parameters(synth_assembly).parameters;
    OperatingEnvironment env;

    // 1. Synthetic assembly rejected under default ExecutionPolicy::engineering_approved_only.
    {
        const auto eng_attempt = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::engineering_approved_only);
        require(!eng_attempt.compatible(), "synthetic rejected in engineering_approved_only mode");
        require(eng_attempt.validation.has_code(
            VerticalValidationCode::synthetic_data_prohibited_in_engineering_mode));
        require(eng_attempt.classification == ExecutionClassification::test_only_synthetic);
    }

    // 2. Synthetic assembly accepted under explicit ExecutionPolicy::test_only_synthetic.
    {
        const auto synth_ok = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(synth_ok.compatible(), "synthetic accepted in test_only_synthetic mode");
        require(synth_ok.classification == ExecutionClassification::test_only_synthetic);
    }

    // 3. Engineering-approved assembly accepted under engineering_approved_only and rejected under test_only_synthetic.
    {
        const auto eng_assembly = make_engineering_approved_rc_assembly();
        const auto eng_resolved = *resolve_rc_parameters(eng_assembly).parameters;

        const auto eng_ok = evaluate_rc_model_compatibility(
            eng_assembly,
            eng_resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::engineering_approved_only);
        require(eng_ok.compatible(), "engineering-approved assembly accepted");
        require(eng_ok.classification == ExecutionClassification::engineering_approved);

        const auto wrong_policy = evaluate_rc_model_compatibility(
            eng_assembly,
            eng_resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!wrong_policy.compatible());
        require(wrong_policy.validation.has_code(
            VerticalValidationCode::inconsistent_synthetic_classification));
    }

    // 4. Reject SPICE subcircuit metadata: do not assume external artifact is available!
    {
        auto subckt_assembly = synth_assembly;
        // Select the Phase 2 capacitor spice_subcircuit model (models[0]) instead of ideal-linear (models[1]).
        subckt_assembly.instances[1].selected_model_id =
            component::ModelArtifactId{"model.synthetic.capacitor.c0g-subckt"};
        subckt_assembly.instances[1].selected_model_version = "1.0.0";
        // Adapt parameter mapping on subckt model to single "C" so resolution succeeds and compatibility gate is tested directly.
        subckt_assembly.component_bundles[1].models[0].parameter_mappings = {
            component::ModelParameterMapping{
                "C",
                component::NormalizedParameterId{"np.cap.c.typ"},
                std::nullopt,
                "F",
                "C = capacitance(typical)"}};
        const auto subckt_res = resolve_rc_parameters(subckt_assembly);
        require(subckt_res.validation.valid(), "subcircuit parameter resolution valid");

        const auto subckt_compat = evaluate_rc_model_compatibility(
            subckt_assembly,
            *subckt_res.parameters,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!subckt_compat.compatible(), "SPICE subcircuit rejected by ideal-RC backend");
        require(subckt_compat.validation.has_code(VerticalValidationCode::unsupported_model_format));
        require(subckt_compat.validation.has_code(
            VerticalValidationCode::external_artifact_unavailable_in_rc_backend));
    }

    // 5. Reject spice_model_card, lookup_table, and vendor_specific_artifact formats.
    for (auto fmt : {component::ModelFormat::spice_model_card,
                     component::ModelFormat::lookup_table,
                     component::ModelFormat::vendor_specific_artifact}) {
        auto fmt_assembly = synth_assembly;
        fmt_assembly.component_bundles[0].models[0].format = fmt;
        const auto compat = evaluate_rc_model_compatibility(
            fmt_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible());
        require(compat.validation.has_code(VerticalValidationCode::unsupported_model_format));
        require(compat.validation.has_code(
            VerticalValidationCode::external_artifact_unavailable_in_rc_backend));
    }

    // 6. Reject unsupported analyses and models missing transient support.
    for (auto bad_analysis : {component::AnalysisType::dc_operating_point,
                              component::AnalysisType::dc_sweep,
                              component::AnalysisType::ac_small_signal,
                              component::AnalysisType::noise,
                              component::AnalysisType::thermal}) {
        const auto compat = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            bad_analysis,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible());
        require(compat.validation.has_code(VerticalValidationCode::unsupported_backend_analysis));
    }
    {
        auto no_tran_model = synth_assembly;
        no_tran_model.component_bundles[0].models[0].supported_analyses = {
            component::AnalysisType::dc_operating_point};
        const auto compat = evaluate_rc_model_compatibility(
            no_tran_model,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible());
        require(compat.validation.has_code(VerticalValidationCode::analysis_not_supported_by_model));
    }

    // 7. Reject unvalidated, failed_validation, and version-mismatched (out_of_date) models.
    {
        auto unval = synth_assembly;
        unval.component_bundles[0].models[0].validation_record_refs = {
            component::ModelValidationId{"mval.synthetic.resistor.initial"}};
        const auto compat_unval = evaluate_rc_model_compatibility(
            unval,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat_unval.compatible());
        require(compat_unval.validation.has_code(VerticalValidationCode::model_unvalidated));

        auto failed_val = synth_assembly;
        failed_val.component_bundles[0].model_validations.back().status =
            component::ModelValidationStatus::failed_validation;
        const auto compat_fail = evaluate_rc_model_compatibility(
            failed_val,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat_fail.compatible());
        require(compat_fail.validation.has_code(VerticalValidationCode::model_failed_validation));

        // Model version bumped to 1.1.0 while ModelValidationRecord still targets 1.0.0.
        auto stale_ver = synth_assembly;
        stale_ver.component_bundles[0].models[0].version = "1.1.0";
        stale_ver.instances[0].selected_model_version = "1.1.0";
        const auto stale_resolved = *resolve_rc_parameters(stale_ver).parameters;
        const auto compat_stale = evaluate_rc_model_compatibility(
            stale_ver,
            stale_resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat_stale.compatible());
        require(compat_stale.validation.has_code(VerticalValidationCode::model_version_mismatch));
        require(compat_stale.validation.has_code(VerticalValidationCode::model_validation_out_of_date));
    }
}

void test_wp3_operating_conditions_and_validity_envelope() {
    const auto synth_assembly = make_validated_synthetic_rc_assembly();
    const auto resolved = *resolve_rc_parameters(synth_assembly).parameters;

    // 1. Out-of-envelope ambient temperature (> 125 degC or < -55 degC).
    for (double bad_temp : {150.0, -60.0}) {
        OperatingEnvironment env;
        env.ambient_temperature_degC = bad_temp;
        const auto compat = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible(), "out-of-envelope temperature rejected");
        require(compat.validation.has_code(
            VerticalValidationCode::operating_condition_outside_validity_envelope));
    }

    // 2. Stimulus voltage exceeding capacitor [-50, 50] V envelope.
    {
        OperatingEnvironment env;
        env.stimulus_voltage_V = 60.0;
        const auto compat = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible(), "60 V exceeds 50 V capacitor validity envelope");
        require(compat.validation.has_code(
            VerticalValidationCode::operating_condition_outside_validity_envelope));
    }

    // 3. Stimulus voltage causing resistor continuous_power V^2/R > 0.050 W (10 V on 1000 ohm = 0.100 W).
    {
        OperatingEnvironment env;
        env.stimulus_voltage_V = 10.0; // Within 50 V capacitor limit, but 0.100 W > 0.050 W resistor envelope!
        const auto compat = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible(), "10 V across 1 kOhm exceeds 50 mW resistor validity envelope");
        require(compat.validation.has_code(
            VerticalValidationCode::operating_condition_outside_validity_envelope));
    }

    // 4. Caller-supplied additional operating condition outside signal_frequency envelope.
    {
        OperatingEnvironment env;
        env.additional_conditions.push_back(component::OperatingCondition{
            "signal_frequency", "5.0e7", "Hz", 5.0e7, "Exceeds 1 MHz resistor model ceiling"});
        const auto compat = evaluate_rc_model_compatibility(
            synth_assembly,
            resolved,
            component::AnalysisType::transient,
            env,
            ExecutionPolicy::test_only_synthetic);
        require(!compat.compatible());
        require(compat.validation.has_code(
            VerticalValidationCode::operating_condition_outside_validity_envelope));
    }

    // 5. Invalid operating conditions (missing unit, non-finite NaN, incompatible unit for envelope quantity).
    {
        OperatingEnvironment env_no_unit;
        env_no_unit.additional_conditions.push_back(component::OperatingCondition{
            "signal_frequency", "1000", "", 1000.0, std::nullopt});
        require(evaluate_rc_model_compatibility(
                    synth_assembly,
                    resolved,
                    component::AnalysisType::transient,
                    env_no_unit,
                    ExecutionPolicy::test_only_synthetic)
                    .validation.has_code(VerticalValidationCode::invalid_operating_condition));

        OperatingEnvironment env_nan;
        env_nan.ambient_temperature_degC = std::numeric_limits<double>::quiet_NaN();
        require(evaluate_rc_model_compatibility(
                    synth_assembly,
                    resolved,
                    component::AnalysisType::transient,
                    env_nan,
                    ExecutionPolicy::test_only_synthetic)
                    .validation.has_code(VerticalValidationCode::invalid_operating_condition));

        OperatingEnvironment env_wrong_unit;
        env_wrong_unit.additional_conditions.push_back(component::OperatingCondition{
            "signal_frequency", "10", "kHz", 10.0, std::nullopt});
        require(evaluate_rc_model_compatibility(
                    synth_assembly,
                    resolved,
                    component::AnalysisType::transient,
                    env_wrong_unit,
                    ExecutionPolicy::test_only_synthetic)
                    .validation.has_code(VerticalValidationCode::invalid_operating_condition));
    }
}

void test_wp4_adapter_and_equivalent_manual_rc_config() {
    const auto synth_assembly = make_validated_synthetic_rc_assembly();
    const auto run_inputs = make_synthetic_test_run_inputs();

    const auto adapted = adapt_vertical_rc_simulation(synth_assembly, run_inputs);
    const RcConfig manual_default{};

    // Compare knowledge-driven configuration against equivalent manually configured RcConfig.
    require(adapted.rc_config.resistance_ohm == manual_default.resistance_ohm, "R_ohm == 1000");
    require(adapted.rc_config.capacitance_F == manual_default.capacitance_F, "C_F == 1e-6");
    require(adapted.rc_config.voltage_V == manual_default.voltage_V, "voltage_V == 5");
    require(adapted.rc_config.initial_voltage_V == manual_default.initial_voltage_V);
    require(adapted.rc_config.delay_s == manual_default.delay_s);
    require(adapted.rc_config.rise_s == manual_default.rise_s);
    require(adapted.rc_config.fall_s == manual_default.fall_s);
    require(adapted.rc_config.width_s == manual_default.width_s);
    require(adapted.rc_config.period_s == manual_default.period_s);
    require(adapted.rc_config.step_s == manual_default.step_s);
    require(adapted.rc_config.duration_s == manual_default.duration_s);

    require(time_constant(adapted.rc_config) == time_constant(manual_default), "identical tau");
    require(config_json(adapted.rc_config) == config_json(manual_default), "identical config_json");
    require(rc_netlist(adapted.rc_config) == rc_netlist(manual_default), "identical SPICE netlist");
    require(
        content_id(rc_netlist(adapted.rc_config)) == content_id(rc_netlist(manual_default)),
        "identical netlist FNV-1a-64 hash");

    // Verify identical analytical and common-grid validation results across all 4001 points.
    const auto grid = validation_grid(manual_default);
    const auto adapted_grid = validation_grid(adapted.rc_config);
    require(grid == adapted_grid, "identical 4001-point common grid");

    Samples manual_samples;
    Samples adapted_samples;
    for (int i = 0; i <= 600; ++i) {
        const double t = manual_default.duration_s * i / 600.0;
        const double vin = (t <= manual_default.delay_s) ? 0.0 : manual_default.voltage_V;
        manual_samples.push_back({t, vin, analytical_voltage(manual_default, t)});
        adapted_samples.push_back({t, vin, analytical_voltage(adapted.rc_config, t)});
        require(
            analytical_voltage(adapted.rc_config, t) == analytical_voltage(manual_default, t),
            "bit-identical analytical voltage");
        require(
            ramp_voltage(adapted.rc_config, t) == ramp_voltage(manual_default, t),
            "bit-identical ramp reference voltage");
    }

    const auto val_manual = validate_on_grid(manual_default, manual_samples, grid);
    const auto val_adapted = validate_on_grid(adapted.rc_config, adapted_samples, adapted_grid);
    require(val_manual.passed && val_adapted.passed, "both pass 1% tolerance");
    require(val_adapted.tolerance_V == 0.05 && val_manual.tolerance_V == 0.05, "unchanged 0.05 V tolerance");
    require(val_adapted.max_absolute_error_V == val_manual.max_absolute_error_V);
    require(val_adapted.rmse_V == val_manual.rmse_V);

    // Reject unsupported solver controls and invalid RC timing configurations.
    {
        auto bad_solver = run_inputs;
        bad_solver.solver.integration_method = "gear";
        require(prepare_vertical_rc_simulation(synth_assembly, bad_solver).validation.has_code(
            VerticalValidationCode::unsupported_solver_controls));

        auto bad_temp_solver = run_inputs;
        bad_temp_solver.solver.ambient_temperature_degC = 50.0; // Within envelope, but != 27 degC solver setting
        require(prepare_vertical_rc_simulation(synth_assembly, bad_temp_solver).validation.has_code(
            VerticalValidationCode::unsupported_solver_controls));

        auto bad_timing = run_inputs;
        bad_timing.transient.step_s = 1e-3; // Violates step_s <= tau / 10
        require(prepare_vertical_rc_simulation(synth_assembly, bad_timing).validation.has_code(
            VerticalValidationCode::invalid_rc_simulation_configuration));
    }
}

void test_wp5_provenance_manifest_and_fail_fast_gate() {
    const auto synth_assembly = make_validated_synthetic_rc_assembly();
    const auto run_inputs = make_synthetic_test_run_inputs();
    const auto adapted = adapt_vertical_rc_simulation(synth_assembly, run_inputs);
    const auto manifest = build_provenance_manifest(synth_assembly, adapted);
    const auto json = provenance_json(manifest);

    require(manifest.circuit_id == CircuitAssemblyId{"circuit.synthetic.rc-lowpass-v1"});
    require(manifest.execution_classification == ExecutionClassification::test_only_synthetic);
    require(manifest.is_synthetic_test_run);
    require(manifest.parameter_origin == "component_knowledge_vertical_slice");

    // Verify resistor provenance lineage.
    require(manifest.resistor.instance_id == "R1");
    require(manifest.resistor.component_id ==
            component::ComponentId{"comp.synthetic.resistor.syn-tf0603-1k00-lf"});
    require(manifest.resistor.part_number == "SYN-TF0603-1K00-LF");
    require(manifest.resistor.part_number_suffix == std::optional<std::string>{"LF"});
    require(manifest.resistor.knowledge_id ==
            component::ComponentKnowledgeId{"ck.synthetic.syn-tf0603-1k00-lf.v1"});
    require(manifest.resistor.knowledge_version == 1U);
    require(manifest.resistor.model_id ==
            component::ModelArtifactId{"model.synthetic.resistor.ohmic-linear"});
    require(manifest.resistor.model_version == "1.0.0");
    require(manifest.resistor.normalized_parameter_id ==
            component::NormalizedParameterId{"np.res.r_dc.typ"});
    require(manifest.resistor.source_observation_id ==
            component::ObservationId{"obs.res.r_dc.typ"});
    require(manifest.resistor.conversion_rule ==
            "explicit_scale: 1.000 kohm * 1000 = 1000.0 ohm");
    require(!manifest.resistor.evidence_location_ids.empty() &&
            manifest.resistor.evidence_location_ids[0] ==
                component::EvidenceId{"ev.res.table3.electrical"});
    require(!manifest.resistor.source_document_ids.empty() &&
            manifest.resistor.source_document_ids[0] ==
                component::SourceDocumentId{"doc.synthetic.syn-tf0603-spec-rev-a"});

    // Verify capacitor provenance lineage.
    require(manifest.capacitor.instance_id == "C1");
    require(manifest.capacitor.component_id ==
            component::ComponentId{"comp.synthetic.capacitor.syn-c0g0805-1u0-j50nt"});
    require(manifest.capacitor.part_number == "SYN-C0G0805-1U0-J50NT");
    require(manifest.capacitor.part_number_suffix == std::optional<std::string>{"J50NT"});
    require(manifest.capacitor.knowledge_id ==
            component::ComponentKnowledgeId{"ck.synthetic.syn-c0g0805-1u0-j50nt.v1"});
    require(manifest.capacitor.knowledge_version == 1U);
    require(manifest.capacitor.model_id ==
            component::ModelArtifactId{"model.synthetic.capacitor.ideal-linear"});
    require(manifest.capacitor.model_version == "1.0.0");
    require(manifest.capacitor.normalized_parameter_id ==
            component::NormalizedParameterId{"np.cap.c.typ"});
    require(manifest.capacitor.source_observation_id ==
            component::ObservationId{"obs.cap.c.typ"});
    require(manifest.capacitor.conversion_rule ==
            "explicit_scale: 1.000 uF * 1e-6 = 1.0e-6 F");
    require(!manifest.capacitor.evidence_location_ids.empty() &&
            manifest.capacitor.evidence_location_ids[0] ==
                component::EvidenceId{"ev.cap.table2.characteristics"});

    // Verify JSON serialization contains key provenance identifiers and hashes.
    require(json.find("\"execution_classification\":\"test_only_synthetic\"") != std::string::npos);
    require(json.find("\"is_synthetic_test_run\":true") != std::string::npos);
    require(json.find("\"comp.synthetic.resistor.syn-tf0603-1k00-lf\"") != std::string::npos);
    require(json.find("\"comp.synthetic.capacitor.syn-c0g0805-1u0-j50nt\"") != std::string::npos);
    require(json.find("\"np.res.r_dc.typ\"") != std::string::npos);
    require(json.find("\"np.cap.c.typ\"") != std::string::npos);
    require(json.find("\"obs.res.r_dc.typ\"") != std::string::npos);
    require(json.find("\"obs.cap.c.typ\"") != std::string::npos);
    require(json.find("\"ev.res.table3.electrical\"") != std::string::npos);
    require(json.find("\"ev.cap.table2.characteristics\"") != std::string::npos);

    // Verify deterministic repeatability of provenance_content_id.
    const auto manifest_repeat = build_provenance_manifest(synth_assembly, adapted);
    require(manifest.provenance_content_id == manifest_repeat.provenance_content_id);
    require(provenance_json(manifest) == provenance_json(manifest_repeat));

    // Verify fail-fast input gate: invalid run inputs must throw BEFORE creating output directory.
    const auto forbidden_dir = std::filesystem::absolute("should_never_be_created_vertical_dir");
    auto bad_inputs = run_inputs;
    bad_inputs.execution_policy = ExecutionPolicy::engineering_approved_only; // Rejects synthetic assembly
    require_throws([&] {
        execute_vertical_rc_run(synth_assembly, bad_inputs, "ngspice", forbidden_dir);
    }, "invalid vertical run fails before creating workspace");
    require(!std::filesystem::exists(forbidden_dir), "no workspace created on pre-execution failure");
}

} // namespace

int main() {
    try {
        test_wp1_circuit_assembly();
        test_wp2_parameter_resolution();
        test_wp3_model_compatibility_and_synthetic_policy();
        test_wp3_operating_conditions_and_validity_envelope();
        test_wp4_adapter_and_equivalent_manual_rc_config();
        test_wp5_provenance_manifest_and_fail_fast_gate();

        std::cout << "PASS: " << g_checks << " vertical slice checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
