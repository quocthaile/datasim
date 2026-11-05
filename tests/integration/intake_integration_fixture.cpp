#include "intake_integration_fixture.hpp"
#include "../unit/intake_fixtures.hpp"
#include "../unit/vertical_slice_fixtures.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/observation/review.hpp"
#include "datasim/intake/intake.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include <stdexcept>

namespace datasim::vertical::testing {
namespace {

component::ComponentDomainBundle review_and_promote(
    const intake::IntakeManifest& manifest,
    const std::string& knowledge_id,
    const std::string& parameter_id,
    const std::string& canonical_name,
    double si_value,
    const std::string& si_unit) {
    const auto result = intake::build_candidate_bundle(manifest);
    if (!result.bundle.has_value()) {
        throw std::runtime_error("synthetic intake fixture did not produce a candidate bundle");
    }

    auto bundle = *result.bundle;
    if (bundle.observations.empty()) {
        throw std::runtime_error("synthetic intake fixture has no observations");
    }
    auto& observation = bundle.observations.front();
    const component::ReviewerIdentity curator{
        "eng.intake.integration", component::ActorKind::human_engineer,
        "Synthetic intake integration reviewer"};
    const component::ReviewDecision queue{
        component::ReviewDecisionId{knowledge_id + ".queue"},
        {"check.intake.integration", component::ActorKind::automated_check,
         "Synthetic intake structural checker"},
        "2026-11-05T09:00:00Z", "Structural intake checks passed.",
        observation.evidence_refs, component::ReviewState::candidate,
        component::ReviewState::needs_review, std::nullopt};
    component::apply_review_decision(observation, queue);
    const component::ReviewDecision approval{
        component::ReviewDecisionId{knowledge_id + ".approve"}, curator,
        "2026-11-05T09:01:00Z",
        "Reviewed synthetic evidence, units, qualifier, and conditions.",
        observation.evidence_refs, component::ReviewState::needs_review,
        component::ReviewState::approved, std::nullopt};
    component::apply_review_decision(observation, approval);

    component::ComponentKnowledge knowledge;
    knowledge.id = component::ComponentKnowledgeId{knowledge_id};
    knowledge.component = bundle.component;
    knowledge.review_decision = {
        component::ReviewDecisionId{knowledge_id + ".knowledge"}, curator,
        "2026-11-05T09:02:00Z", "Promoted reviewed synthetic intake evidence.",
        observation.evidence_refs, component::ReviewState::needs_review,
        component::ReviewState::approved, std::nullopt};
    knowledge.version_history.push_back({
        1U, knowledge.id, std::nullopt, curator, "2026-11-05T09:02:00Z",
        "Initial synthetic intake knowledge snapshot."});
    component::promote_parameter_to_knowledge(
        knowledge, observation,
        component::make_normalized_parameter(
            component::NormalizedParameterId{parameter_id}, observation,
            canonical_name, canonical_name, si_value, si_unit, false,
            "explicit synthetic fixture scale", observation.operating_conditions));
    bundle.knowledge = knowledge;
    return bundle;
}

void rebind_model(component::ComponentDomainBundle& bundle,
                  const component::ModelArtifactDescriptor& template_model,
                  const std::vector<component::ModelValidationRecord>& template_records) {
    auto model = template_model;
    model.component_id = bundle.component.id;
    model.component_knowledge_id = bundle.knowledge->id;
    model.parameter_mappings.front().knowledge_param_id =
        bundle.knowledge->normalized_parameters.front().id;
    model.validation_record_refs.clear();
    bundle.models = {model};
    bundle.model_validations.clear();
    for (const auto& template_record : template_records) {
        if (template_record.model_id != template_model.id) {
            continue;
        }
        auto record = template_record;
        record.model_id = model.id;
        record.evidence_refs = {bundle.observations.front().evidence_refs.front()};
        bundle.model_validations.push_back(record);
        model.validation_record_refs.push_back(record.id);
    }
    bundle.models.front().validation_record_refs = model.validation_record_refs;
}

} // namespace

CircuitAssembly make_intake_derived_rc_assembly() {
    const auto resistor_manifest = intake::testing::make_synthetic_resistor_manifest();
    const auto capacitor_manifest = intake::testing::make_synthetic_capacitor_manifest();
    auto resistor = review_and_promote(
        resistor_manifest, "ck.intake.integration.resistor.v1",
        "np.intake.integration.resistor.typ", "dc_resistance", 1000.0, "ohm");
    auto capacitor = review_and_promote(
        capacitor_manifest, "ck.intake.integration.capacitor.v1",
        "np.intake.integration.capacitor.typ", "capacitance", 1.0e-6, "F");

    const auto model_templates = make_validated_synthetic_rc_assembly();
    const auto& resistor_template = model_templates.component_bundles.front();
    const auto& capacitor_template = model_templates.component_bundles.back();
    rebind_model(resistor, resistor_template.models.front(),
                 resistor_template.model_validations);
    rebind_model(capacitor, capacitor_template.models.back(),
                 capacitor_template.model_validations);

    CircuitAssembly assembly = model_templates;
    assembly.id = CircuitAssemblyId{"circuit.intake.synthetic.rc-lowpass-v1"};
    assembly.name = "Synthetic intake-derived RC low-pass step-response circuit";
    assembly.component_bundles = {std::move(resistor), std::move(capacitor)};
    assembly.instances.front().component_id = assembly.component_bundles.front().component.id;
    assembly.instances.front().knowledge_id = assembly.component_bundles.front().knowledge->id;
    assembly.instances.front().selected_model_id =
        assembly.component_bundles.front().models.front().id;
    assembly.instances.front().selected_model_version =
        assembly.component_bundles.front().models.front().version;
    assembly.instances.back().component_id = assembly.component_bundles.back().component.id;
    assembly.instances.back().knowledge_id = assembly.component_bundles.back().knowledge->id;
    assembly.instances.back().selected_model_id =
        assembly.component_bundles.back().models.front().id;
    assembly.instances.back().selected_model_version =
        assembly.component_bundles.back().models.front().version;
    return assembly;
}

} // namespace datasim::vertical::testing
