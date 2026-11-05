#include "pdf_simulation_gate.hpp"
#include "../unit/vertical_slice_fixtures.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/model/model_validation.hpp"
#include "datasim/component/observation/review.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include "datasim/intake/intake.hpp"
#include "datasim/pdf/table_extraction.hpp"
#include "datasim/simulation/backend/process.hpp"
#include "datasim/simulation/io/report.hpp"
#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include "datasim/vertical/compatibility/model_compatibility.hpp"
#include "datasim/vertical/provenance/run_provenance.hpp"
#include "datasim/vertical/resolution/parameter_resolution.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef DATASIM_SYNTHETIC_PDF_PATH
#error DATASIM_SYNTHETIC_PDF_PATH must be defined
#endif
#ifndef DATASIM_IMAGE_ONLY_PDF_PATH
#error DATASIM_IMAGE_ONLY_PDF_PATH must be defined
#endif

namespace datasim::vertical::testing {
namespace {

namespace fs = std::filesystem;

void gate_require(bool condition, const std::string& message, std::size_t& counter) {
    ++counter;
    if (!condition) {
        throw std::runtime_error("Phase 4D gate failure: " + message);
    }
}

std::string read_bytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        throw std::runtime_error("missing Phase 4D artifact: " + p.string());
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

pdf::ExtractionRequest make_table_request(
    const fs::path& pdf_path,
    const fs::path& pdfinfo_path,
    const fs::path& pdftotext_path,
    std::size_t page_index,
    const std::string& family,
    const std::string& mpn,
    const std::string& document_id) {
    pdf::ExtractionRequest request;
    request.pdf_path = pdf_path;
    request.pdfinfo_path = pdfinfo_path;
    request.pdftotext_path = pdftotext_path;
    request.pdf_page_index = page_index;
    request.printed_page_label = (page_index == 0U) ? "5" : "Appendix C-2";
    request.curator = {
        "curator.pdf.synthetic.gate",
        component::ActorKind::human_engineer,
        "Synthetic PDF Gate Intake Curator (Test Fixture Only)"};
    request.component = {
        component::ComponentId{"cmp.fixture." + family},
        "Datasim Synthetic",
        mpn,
        std::nullopt,
        "fixture-package",
        (family == "resistor") ? component::ComponentFamily::resistor
                               : component::ComponentFamily::capacitor,
        std::nullopt,
        true};
    request.source_document = {
        component::SourceDocumentId{document_id},
        "Datasim Synthetic",
        "Fictional electrical characteristics",
        {mpn},
        "fixture-rev-1",
        std::nullopt,
        std::nullopt,
        std::nullopt,
        component::DocumentUsageRights{
            std::nullopt,
            false,
            "Generated fictional PDF for automated testing."},
        true};
    request.table.table_id = (family == "resistor") ? "TABLE_R" : "TABLE_C";
    request.table.region_x_min = 20.0;
    request.table.region_y_min = 80.0;
    request.table.region_x_max = 690.0;
    request.table.region_y_max = 215.0;
    request.table.row_y_tolerance = 2.0;
    request.table.header_y_min = 80.0;
    request.table.header_y_max = 110.0;
    request.table.parameter_column = {20.0, 215.0};
    request.table.unit_column = {215.0, 285.0};
    request.table.minimum_column = {285.0, 350.0};
    request.table.typical_column = {350.0, 420.0};
    request.table.maximum_column = {420.0, 490.0};
    request.table.condition_column = pdf::HorizontalSpan{490.0, 690.0};
    request.table.category = component::ParameterCategory::electrical_characteristic;
    return request;
}

component::ParameterObservation& find_observation_mut(
    component::ComponentDomainBundle& bundle,
    std::string_view obs_id) {
    for (auto& obs : bundle.observations) {
        if (obs.id.value == obs_id) {
            return obs;
        }
    }
    throw std::runtime_error("expected PDF observation not found: " + std::string(obs_id));
}

const component::ParameterObservation& find_observation_const(
    const component::ComponentDomainBundle& bundle,
    std::string_view obs_id) {
    for (const auto& obs : bundle.observations) {
        if (obs.id.value == obs_id) {
            return obs;
        }
    }
    throw std::runtime_error("expected PDF observation not found: " + std::string(obs_id));
}

void queue_and_decide_observation(
    component::ParameterObservation& obs,
    const std::string& decision_prefix,
    const component::ReviewerIdentity& human_reviewer,
    component::ReviewState final_state,
    const std::string& rationale) {
    const component::ReviewerIdentity checker{
        "check.pdf.synthetic.gate",
        component::ActorKind::automated_check,
        "Synthetic PDF Gate Structural Intake Checker"};
    const component::ReviewDecision queue_dec{
        component::ReviewDecisionId{decision_prefix + ".queue"},
        checker,
        "2026-11-05T10:00:00Z",
        "Queued deterministic PDF table observation for synthetic engineering review.",
        obs.evidence_refs,
        component::ReviewState::candidate,
        component::ReviewState::needs_review,
        std::nullopt};
    component::apply_review_decision(obs, queue_dec);

    const component::ReviewDecision final_dec{
        component::ReviewDecisionId{decision_prefix + ".final"},
        human_reviewer,
        "2026-11-05T10:05:00Z",
        rationale,
        obs.evidence_refs,
        component::ReviewState::needs_review,
        final_state,
        std::nullopt};
    component::apply_review_decision(obs, final_dec);
}

std::vector<component::EvidenceId> collect_bundle_evidence_ids(
    const component::ComponentDomainBundle& bundle) {
    std::vector<component::EvidenceId> ids;
    ids.reserve(bundle.evidence_locations.size());
    for (const auto& ev : bundle.evidence_locations) {
        ids.push_back(ev.id);
    }
    return ids;
}

component::ComponentDomainBundle build_reviewed_resistor_bundle_from_pdf(
    const pdf::ExtractionResult& extraction,
    std::size_t& checks) {
    gate_require(extraction.manifest.has_value(), "resistor extraction manifest present", checks);
    gate_require(
        extraction.candidate_bundle.has_value(),
        "resistor extraction candidate_bundle present",
        checks);

    // Step 4(3): Pass through existing intake validation and candidate-bundle boundaries.
    const auto manifest_report = intake::validate_intake_manifest(*extraction.manifest);
    gate_require(manifest_report.valid(), "resistor intake manifest validates cleanly", checks);
    gate_require(
        manifest_report.has_code(intake::IntakeDiagnosticCode::conflict_candidate),
        "resistor intake manifest retains conflict_candidate diagnostic for 25 degC duplicate row",
        checks);
    const auto rebuilt = intake::build_candidate_bundle(*extraction.manifest);
    gate_require(
        rebuilt.report.valid() && rebuilt.bundle.has_value(),
        "resistor build_candidate_bundle succeeds",
        checks);
    gate_require(
        *rebuilt.bundle == *extraction.candidate_bundle,
        "resistor build_candidate_bundle matches extraction candidate_bundle",
        checks);

    auto bundle = *rebuilt.bundle;
    gate_require(bundle.observations.size() == 9U, "resistor bundle has 9 observations", checks);
    gate_require(
        !bundle.knowledge.has_value() && bundle.models.empty() && bundle.model_validations.empty(),
        "resistor candidate bundle has no knowledge, models, or model validations",
        checks);

    // Step 4(4): Verify that all extracted observations remain candidates with empty review history.
    for (const auto& obs : bundle.observations) {
        gate_require(
            obs.review_state == component::ReviewState::candidate,
            "resistor extracted observation starts in ReviewState::candidate",
            checks);
        gate_require(
            obs.review_history.empty(),
            "resistor extracted observation starts with empty review_history",
            checks);
        gate_require(
            obs.extraction_method == component::ExtractionMethod::deterministic_table_parser,
            "resistor observation preserves deterministic_table_parser extraction_method",
            checks);
        gate_require(
            obs.extraction_tool == "Poppler pdftotext -bbox-layout" &&
                obs.extraction_tool_version.has_value() &&
                !obs.extraction_tool_version->empty(),
            "resistor observation preserves Poppler tool provenance",
            checks);
    }

    const component::ReviewerIdentity reviewer{
        "eng.synthetic.pdf-gate-reviewer",
        component::ActorKind::human_engineer,
        "Synthetic Test Fixture Engineering Reviewer (Not Real Human Authorization)"};

    // Row 1 (T_A=25 degC; V_DD=5 V): approved observations; r1.typ selected over conflicting r3.typ.
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r1.min"),
        "rev.pdf.res.r1.min",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 1 minimum resistance (9.9 kohm at T_A=25 degC; V_DD=5 V).");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r1.typ"),
        "rev.pdf.res.r1.typ",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 1 nominal resistance (10.0 kohm at T_A=25 degC; V_DD=5 V) over conflicting Row 3.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r1.max"),
        "rev.pdf.res.r1.max",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 1 maximum resistance (10.1 kohm at T_A=25 degC; V_DD=5 V).");

    // Row 2 (T_A=85 degC): unambiguous, condition-compatible row selected for RC assembly promotion.
    auto& obs_r2_min = find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r2.min");
    auto& obs_r2_typ = find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r2.typ");
    auto& obs_r2_max = find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r2.max");
    queue_and_decide_observation(
        obs_r2_min,
        "rev.pdf.res.r2.min",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 2 minimum resistance (9.8 kohm at T_A=85 degC).");
    queue_and_decide_observation(
        obs_r2_typ,
        "rev.pdf.res.r2.typ",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 2 typical resistance (10.0 kohm at T_A=85 degC) for condition-compatible RC assembly.");
    queue_and_decide_observation(
        obs_r2_max,
        "rev.pdf.res.r2.max",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 2 maximum resistance (10.2 kohm at T_A=85 degC).");

    // Row 3 (T_A=25 degC; V_DD=5 V): rejected conflicting duplicate row preserved in bundle.observations.
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r3.min"),
        "rev.pdf.res.r3.min",
        reviewer,
        component::ReviewState::rejected,
        "Rejected duplicate 25 degC Row 3 minimum entry associated with conflicting typical row.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r3.typ"),
        "rev.pdf.res.r3.typ",
        reviewer,
        component::ReviewState::rejected,
        "Rejected conflicting 25 degC Row 3 typical resistance (10.01 kohm vs Row 1 10.0 kohm).");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r3.max"),
        "rev.pdf.res.r3.max",
        reviewer,
        component::ReviewState::rejected,
        "Rejected duplicate 25 degC Row 3 maximum entry associated with conflicting typical row.");

    component::ComponentKnowledge kn;
    kn.id = component::ComponentKnowledgeId{"ck.pdf.synthetic.resistor.v1"};
    kn.component = bundle.component;
    kn.version = 1U;
    kn.previous_version_id = std::nullopt;
    kn.review_decision = component::ReviewDecision{
        component::ReviewDecisionId{"rev.pdf.res.knowledge.v1"},
        reviewer,
        "2026-11-05T10:10:00Z",
        "Promoted synthetic PDF-extracted resistor Row 2 (T_A=85 degC) parameters with documented Row 1/Row 3 conflict resolution.",
        collect_bundle_evidence_ids(bundle),
        component::ReviewState::needs_review,
        component::ReviewState::approved,
        std::nullopt};
    kn.version_history.push_back(component::KnowledgeRevisionEntry{
        1U,
        kn.id,
        std::nullopt,
        reviewer,
        "2026-11-05T10:10:00Z",
        "Initial synthetic PDF-extracted resistor knowledge snapshot."});

    // Preserve the 25 degC conflict explicitly in ComponentKnowledge.conflicts.
    kn.conflicts.push_back(component::ParameterConflict{
        "conflict.pdf.resistor.r_dc.25c",
        "Resistance",
        {component::ObservationId{"obs.doc.synthetic.rc.TABLE_R.p0.r1.typ"},
         component::ObservationId{"obs.doc.synthetic.rc.TABLE_R.p0.r3.typ"}},
        component::ConflictResolutionStatus::resolved_by_review,
        component::ObservationId{"obs.doc.synthetic.rc.TABLE_R.p0.r1.typ"},
        "Competing typical Resistance rows at T_A=25 degC; V_DD=5 V (10.0 kohm in Row 1 vs 10.01 kohm in Row 3).",
        reviewer,
        "Selected Row 1 (10.0 kohm) and rejected Row 3 (10.01 kohm) in synthetic review; promoted unambiguous Row 2 (T_A=85 degC) for condition-compatible RC assembly."});

    // Step 4(8): Explicit SI unit normalization (kohm -> ohm).
    component::promote_parameter_to_knowledge(
        kn,
        obs_r2_min,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.resistor.r_dc.min"},
            obs_r2_min,
            "dc_resistance",
            "Minimum DC electrical resistance at T_A=85 degC",
            9800.0,
            "ohm",
            false,
            "explicit_scale: 9.8 kohm * 1000 = 9800.0 ohm",
            obs_r2_min.operating_conditions));
    component::promote_parameter_to_knowledge(
        kn,
        obs_r2_typ,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.resistor.r_dc.typ"},
            obs_r2_typ,
            "dc_resistance",
            "Nominal/typical DC electrical resistance at T_A=85 degC",
            10000.0,
            "ohm",
            false,
            "explicit_scale: 10.0 kohm * 1000 = 10000.0 ohm",
            obs_r2_typ.operating_conditions));
    component::promote_parameter_to_knowledge(
        kn,
        obs_r2_max,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.resistor.r_dc.max"},
            obs_r2_max,
            "dc_resistance",
            "Maximum DC electrical resistance at T_A=85 degC",
            10200.0,
            "ohm",
            false,
            "explicit_scale: 10.2 kohm * 1000 = 10200.0 ohm",
            obs_r2_max.operating_conditions));

    bundle.knowledge = kn;

    // Step 4(9): Construct explicitly selected, independently validated synthetic resistor model.
    const auto template_assembly = make_validated_synthetic_rc_assembly();
    auto res_model = template_assembly.component_bundles.front().models.front();
    res_model.id = component::ModelArtifactId{"model.synthetic.pdf.resistor.ohmic-linear"};
    res_model.component_id = bundle.component.id;
    res_model.component_knowledge_id = kn.id;
    res_model.parameter_mappings.front().knowledge_param_id =
        component::NormalizedParameterId{"np.pdf.resistor.r_dc.typ"};
    res_model.validation_record_refs = {
        component::ModelValidationId{"mval.synthetic.pdf.resistor.validated.v1"}};
    res_model.is_synthetic = true;

    const component::ReviewerIdentity model_validator{
        "eng.synthetic.pdf-model-validator",
        component::ActorKind::human_engineer,
        "Synthetic Test Fixture Model Qualification Engineer"};
    component::ModelValidationRecord res_val;
    res_val.id = component::ModelValidationId{"mval.synthetic.pdf.resistor.validated.v1"};
    res_val.model_id = res_model.id;
    res_val.model_version = res_model.version;
    res_val.status = component::ModelValidationStatus::validated_within_envelope;
    res_val.validation_kind = component::ModelValidationKind::analytical_comparison;
    res_val.reviewer = model_validator;
    res_val.timestamp = "2026-11-05T10:15:00Z";
    res_val.summary =
        "Validated synthetic ideal ohmic resistor model for SIM-R-10K against closed-form V = R * I within envelope.";
    res_val.evidence_refs = obs_r2_typ.evidence_refs;
    res_val.verified_analyses = {
        component::AnalysisType::dc_operating_point,
        component::AnalysisType::transient};

    bundle.models = {res_model};
    bundle.model_validations = {res_val};

    const auto domain_rep = component::validate_domain_bundle(bundle);
    gate_require(domain_rep.valid(), "reviewed PDF resistor bundle passes domain validation", checks);
    return bundle;
}

component::ComponentDomainBundle build_reviewed_capacitor_bundle_from_pdf(
    const pdf::ExtractionResult& extraction,
    std::size_t& checks) {
    gate_require(extraction.manifest.has_value(), "capacitor extraction manifest present", checks);
    gate_require(
        extraction.candidate_bundle.has_value(),
        "capacitor extraction candidate_bundle present",
        checks);

    // Step 4(3): Pass through existing intake validation and candidate-bundle boundaries.
    const auto manifest_report = intake::validate_intake_manifest(*extraction.manifest);
    gate_require(manifest_report.valid(), "capacitor intake manifest validates cleanly", checks);
    gate_require(
        manifest_report.has_code(intake::IntakeDiagnosticCode::ambiguous_numeric_text),
        "capacitor intake manifest retains ambiguous_numeric_text diagnostics",
        checks);
    const auto rebuilt = intake::build_candidate_bundle(*extraction.manifest);
    gate_require(
        rebuilt.report.valid() && rebuilt.bundle.has_value(),
        "capacitor build_candidate_bundle succeeds",
        checks);
    gate_require(
        *rebuilt.bundle == *extraction.candidate_bundle,
        "capacitor build_candidate_bundle matches extraction candidate_bundle",
        checks);

    auto bundle = *rebuilt.bundle;
    gate_require(bundle.observations.size() == 11U, "capacitor bundle has 11 observations", checks);
    gate_require(
        !bundle.knowledge.has_value() && bundle.models.empty() && bundle.model_validations.empty(),
        "capacitor candidate bundle has no knowledge, models, or model validations",
        checks);

    for (const auto& obs : bundle.observations) {
        gate_require(
            obs.review_state == component::ReviewState::candidate,
            "capacitor extracted observation starts in ReviewState::candidate",
            checks);
        gate_require(
            obs.review_history.empty(),
            "capacitor extracted observation starts with empty review_history",
            checks);
    }

    const component::ReviewerIdentity reviewer{
        "eng.synthetic.pdf-gate-reviewer",
        component::ActorKind::human_engineer,
        "Synthetic Test Fixture Engineering Reviewer (Not Real Human Authorization)"};

    // Row 1 (T_A=25 degC): r1.typ ("1,000") rejected due to ambiguous comma separator; r1.max approved.
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r1.typ"),
        "rev.pdf.cap.r1.typ",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 1 typical capacitance '1,000' uF due to ambiguous comma grouping.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r1.max"),
        "rev.pdf.cap.r1.max",
        reviewer,
        component::ReviewState::approved,
        "Approved Row 1 maximum capacitance (1.1 uF at T_A=25 degC); not promoted for 85 degC nominal run.");

    // Row 2 (T_A=85 degC, footnote "*"): rejected because typical cell carries unresolved footnote marker "1.0*".
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r2.min"),
        "rev.pdf.cap.r2.min",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 2 minimum capacitance belonging to footnote-qualified row.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r2.typ"),
        "rev.pdf.cap.r2.typ",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 2 typical capacitance '1.0*' uF because footnote-qualified text is unparsed.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r2.max"),
        "rev.pdf.cap.r2.max",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 2 maximum capacitance belonging to footnote-qualified row.");

    // Row 3 (T_A=85 degC): unambiguous, condition-compatible row selected for RC assembly promotion.
    auto& obs_r3_min = find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r3.min");
    auto& obs_r3_typ = find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r3.typ");
    auto& obs_r3_max = find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r3.max");
    queue_and_decide_observation(
        obs_r3_min,
        "rev.pdf.cap.r3.min",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 3 minimum capacitance (0.9 uF at T_A=85 degC).");
    queue_and_decide_observation(
        obs_r3_typ,
        "rev.pdf.cap.r3.typ",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 3 typical capacitance (1.0 uF at T_A=85 degC) for condition-compatible RC assembly.");
    queue_and_decide_observation(
        obs_r3_max,
        "rev.pdf.cap.r3.max",
        reviewer,
        component::ReviewState::approved,
        "Approved synthetic Row 3 maximum capacitance (1.1 uF at T_A=85 degC).");

    // Row 4 (T_A=85 degC, blank unit): rejected because unit cell is blank and no unit is inferred.
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r4.min"),
        "rev.pdf.cap.r4.min",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 4 minimum entry because unit column is blank.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r4.typ"),
        "rev.pdf.cap.r4.typ",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 4 typical entry because unit column is blank.");
    queue_and_decide_observation(
        find_observation_mut(bundle, "obs.doc.synthetic.cap.TABLE_C.p1.r4.max"),
        "rev.pdf.cap.r4.max",
        reviewer,
        component::ReviewState::rejected,
        "Rejected Row 4 maximum entry because unit column is blank.");

    component::ComponentKnowledge kn;
    kn.id = component::ComponentKnowledgeId{"ck.pdf.synthetic.capacitor.v1"};
    kn.component = bundle.component;
    kn.version = 1U;
    kn.previous_version_id = std::nullopt;
    kn.review_decision = component::ReviewDecision{
        component::ReviewDecisionId{"rev.pdf.cap.knowledge.v1"},
        reviewer,
        "2026-11-05T10:12:00Z",
        "Promoted synthetic PDF-extracted capacitor Row 3 (T_A=85 degC) parameters after rejecting ambiguous/footnote/unitless rows.",
        collect_bundle_evidence_ids(bundle),
        component::ReviewState::needs_review,
        component::ReviewState::approved,
        std::nullopt};
    kn.version_history.push_back(component::KnowledgeRevisionEntry{
        1U,
        kn.id,
        std::nullopt,
        reviewer,
        "2026-11-05T10:12:00Z",
        "Initial synthetic PDF-extracted capacitor knowledge snapshot."});

    // Step 4(8): Explicit SI unit normalization (uF -> F).
    component::promote_parameter_to_knowledge(
        kn,
        obs_r3_min,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.capacitor.c.min"},
            obs_r3_min,
            "capacitance",
            "Minimum small-signal capacitance at T_A=85 degC",
            0.9e-6,
            "F",
            false,
            "explicit_scale: 0.9 uF * 1e-6 = 9.0e-7 F",
            obs_r3_min.operating_conditions));
    component::promote_parameter_to_knowledge(
        kn,
        obs_r3_typ,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.capacitor.c.typ"},
            obs_r3_typ,
            "capacitance",
            "Nominal/typical capacitance at T_A=85 degC",
            1.0e-6,
            "F",
            false,
            "explicit_scale: 1.0 uF * 1e-6 = 1.0e-6 F",
            obs_r3_typ.operating_conditions));
    component::promote_parameter_to_knowledge(
        kn,
        obs_r3_max,
        component::make_normalized_parameter(
            component::NormalizedParameterId{"np.pdf.capacitor.c.max"},
            obs_r3_max,
            "capacitance",
            "Maximum small-signal capacitance at T_A=85 degC",
            1.1e-6,
            "F",
            false,
            "explicit_scale: 1.1 uF * 1e-6 = 1.1e-6 F",
            obs_r3_max.operating_conditions));

    bundle.knowledge = kn;

    // Step 4(9): Construct explicitly selected, independently validated synthetic capacitor model.
    const auto template_assembly = make_validated_synthetic_rc_assembly();
    auto cap_model = template_assembly.component_bundles.back().models.back();
    cap_model.id = component::ModelArtifactId{"model.synthetic.pdf.capacitor.ideal-linear"};
    cap_model.component_id = bundle.component.id;
    cap_model.component_knowledge_id = kn.id;
    cap_model.parameter_mappings.front().knowledge_param_id =
        component::NormalizedParameterId{"np.pdf.capacitor.c.typ"};
    cap_model.validation_record_refs = {
        component::ModelValidationId{"mval.synthetic.pdf.capacitor.validated.v1"}};
    cap_model.is_synthetic = true;

    const component::ReviewerIdentity model_validator{
        "eng.synthetic.pdf-model-validator",
        component::ActorKind::human_engineer,
        "Synthetic Test Fixture Model Qualification Engineer"};
    component::ModelValidationRecord cap_val;
    cap_val.id = component::ModelValidationId{"mval.synthetic.pdf.capacitor.validated.v1"};
    cap_val.model_id = cap_model.id;
    cap_val.model_version = cap_model.version;
    cap_val.status = component::ModelValidationStatus::validated_within_envelope;
    cap_val.validation_kind = component::ModelValidationKind::analytical_comparison;
    cap_val.reviewer = model_validator;
    cap_val.timestamp = "2026-11-05T10:16:00Z";
    cap_val.summary =
        "Validated synthetic ideal linear capacitor model for SIM-C-1UF against closed-form RC step response within envelope.";
    cap_val.evidence_refs = obs_r3_typ.evidence_refs;
    cap_val.verified_analyses = {
        component::AnalysisType::dc_operating_point,
        component::AnalysisType::transient};

    bundle.models = {cap_model};
    bundle.model_validations = {cap_val};

    const auto domain_rep = component::validate_domain_bundle(bundle);
    gate_require(domain_rep.valid(), "reviewed PDF capacitor bundle passes domain validation", checks);
    return bundle;
}

CircuitAssembly assemble_pdf_rc_circuit(
    component::ComponentDomainBundle resistor_bundle,
    component::ComponentDomainBundle capacitor_bundle) {
    CircuitComponentInstance r1;
    r1.instance_id = ComponentInstanceId{"R1"};
    r1.role = CircuitElementRole::series_resistor;
    r1.component_id = resistor_bundle.component.id;
    r1.knowledge_id = resistor_bundle.knowledge->id;
    r1.expected_knowledge_version = resistor_bundle.knowledge->version;
    r1.selected_model_id = resistor_bundle.models.front().id;
    r1.selected_model_version = resistor_bundle.models.front().version;
    r1.terminal_bindings = {
        TerminalNodeBinding{1U, "p1", "1", "vin"},
        TerminalNodeBinding{2U, "p2", "2", "vout"}};

    CircuitComponentInstance c1;
    c1.instance_id = ComponentInstanceId{"C1"};
    c1.role = CircuitElementRole::shunt_capacitor;
    c1.component_id = capacitor_bundle.component.id;
    c1.knowledge_id = capacitor_bundle.knowledge->id;
    c1.expected_knowledge_version = capacitor_bundle.knowledge->version;
    c1.selected_model_id = capacitor_bundle.models.front().id;
    c1.selected_model_version = capacitor_bundle.models.front().version;
    c1.terminal_bindings = {
        TerminalNodeBinding{1U, "port_a", "1", "vout"},
        TerminalNodeBinding{2U, "port_b", "2", "0"}};

    CircuitAssembly assembly;
    assembly.id = CircuitAssemblyId{"circuit.pdf.synthetic.rc-lowpass-v1"};
    assembly.name = "Synthetic PDF-Extracted RC Low-Pass Step-Response Circuit";
    assembly.topology = CircuitTopologyKind::series_rc_lowpass;
    assembly.input_node = "vin";
    assembly.output_node = "vout";
    assembly.ground_node = "0";
    assembly.component_bundles = {std::move(resistor_bundle), std::move(capacitor_bundle)};
    assembly.instances = {std::move(r1), std::move(c1)};
    return assembly;
}

void run_negative_and_trust_boundary_tests(
    const fs::path& pdf_path,
    const fs::path& image_only_pdf_path,
    const fs::path& pdfinfo_path,
    const fs::path& pdftotext_path,
    const pdf::ExtractionResult& res_extraction,
    const pdf::ExtractionResult& cap_extraction,
    const CircuitAssembly& valid_pdf_assembly,
    const fs::path& workspace_dir,
    std::size_t& checks) {
    const auto v_inputs = make_synthetic_test_run_inputs();

    // Negative Case 1: Incorrect exact MPN or source-document association.
    {
        auto bad_mpn_req = make_table_request(
            pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K-WRONG", "doc.synthetic.bad-mpn");
        bad_mpn_req.source_document.associated_part_numbers = {"SIM-R-10K"};
        const auto bad_mpn_res = pdf::extract_electrical_table(bad_mpn_req);
        gate_require(
            !bad_mpn_res.candidate_bundle.has_value() && !bad_mpn_res.report.valid(),
            "N-01a: mismatched exact MPN fails closed without candidate bundle",
            checks);

        auto bad_pub_req = make_table_request(
            pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K", "doc.synthetic.bad-pub");
        bad_pub_req.source_document.publisher = "Unrelated Third-Party Publisher";
        const auto bad_pub_res = pdf::extract_electrical_table(bad_pub_req);
        gate_require(
            !bad_pub_res.candidate_bundle.has_value() &&
                bad_pub_res.report.has_code(intake::IntakeDiagnosticCode::manufacturer_publisher_mismatch),
            "N-01b: undeclared manufacturer/publisher mismatch fails closed",
            checks);

        auto bad_suffix_req = make_table_request(
            pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K", "doc.synthetic.bad-suffix");
        bad_suffix_req.component.part_number_suffix = "TR";
        const auto bad_suffix_res = pdf::extract_electrical_table(bad_suffix_req);
        gate_require(
            !bad_suffix_res.candidate_bundle.has_value() &&
                bad_suffix_res.report.has_code(intake::IntakeDiagnosticCode::part_number_suffix_mismatch),
            "N-01c: part_number_suffix mismatch fails closed",
            checks);
    }

    // Negative Case 2: Invalid PDF page index.
    {
        auto bad_page_req = make_table_request(
            pdf_path, pdfinfo_path, pdftotext_path, 2U, "resistor", "SIM-R-10K", "doc.synthetic.bad-page");
        const auto bad_page_res = pdf::extract_electrical_table(bad_page_req);
        gate_require(
            !bad_page_res.candidate_bundle.has_value() &&
                !bad_page_res.manifest.has_value() &&
                !bad_page_res.diagnostics.empty(),
            "N-02a: out-of-range PDF page index fails closed at extraction",
            checks);

        auto bad_manifest = *res_extraction.manifest;
        bad_manifest.evidence.front().location.pdf_page_index = 5U;
        const auto bad_page_bundle = intake::build_candidate_bundle(bad_manifest);
        gate_require(
            !bad_page_bundle.bundle.has_value() &&
                bad_page_bundle.report.has_code(intake::IntakeDiagnosticCode::page_index_out_of_range),
            "N-02b: out-of-range PDF page index in manifest fails closed at intake",
            checks);
    }

    // Negative Case 3: Missing selectable PDF text (image-only PDF and empty region).
    {
        auto img_req = make_table_request(
            image_only_pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K", "doc.synthetic.image");
        const auto img_res = pdf::extract_electrical_table(img_req);
        gate_require(
            !img_res.candidate_bundle.has_value() &&
                !img_res.manifest.has_value() &&
                !img_res.diagnostics.empty() &&
                img_res.diagnostics.front().find("no selectable text") != std::string::npos,
            "N-03a: image-only PDF page fails closed without OCR fallback",
            checks);

        auto empty_region_req = make_table_request(
            pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K", "doc.synthetic.empty-region");
        empty_region_req.table.region_x_min = 1.0;
        empty_region_req.table.region_x_max = 10.0;
        const auto empty_region_res = pdf::extract_electrical_table(empty_region_req);
        gate_require(
            !empty_region_res.candidate_bundle.has_value() &&
                !empty_region_res.diagnostics.empty(),
            "N-03b: table region without selectable text fails closed",
            checks);
    }

    // Negative Case 4: Ambiguous numeric value with no trustworthy parsed scalar.
    {
        const auto& cap_bundle = *cap_extraction.candidate_bundle;
        const auto& obs_comma = find_observation_const(
            cap_bundle,
            "obs.doc.synthetic.cap.TABLE_C.p1.r1.typ");
        const auto& obs_footnote = find_observation_const(
            cap_bundle,
            "obs.doc.synthetic.cap.TABLE_C.p1.r2.typ");
        const auto& obs_blank_unit = find_observation_const(
            cap_bundle,
            "obs.doc.synthetic.cap.TABLE_C.p1.r4.typ");
        gate_require(
            obs_comma.original_value_text == "1,000" && !obs_comma.parsed_numeric_value.has_value(),
            "N-04a: comma-grouped numeric text '1,000' has no parsed scalar",
            checks);
        gate_require(
            obs_footnote.original_value_text == "1.0*" && !obs_footnote.parsed_numeric_value.has_value(),
            "N-04b: footnote-marked numeric text '1.0*' has no parsed scalar",
            checks);
        gate_require(
            !obs_blank_unit.original_unit_symbol.has_value() && !obs_blank_unit.parsed_numeric_value.has_value(),
            "N-04c: blank-unit cell has no inferred unit or parsed scalar",
            checks);

        // Attempting to assert a scalar on ambiguous text in IntakeManifest fails closed.
        auto coerced_manifest = *cap_extraction.manifest;
        for (auto& entry : coerced_manifest.observations) {
            if (entry.observation.id.value == "obs.doc.synthetic.cap.TABLE_C.p1.r1.typ") {
                entry.observation.parsed_numeric_value = 1000.0;
            }
        }
        const auto coerced_result = intake::build_candidate_bundle(coerced_manifest);
        gate_require(
            !coerced_result.bundle.has_value() &&
                coerced_result.report.has_code(intake::IntakeDiagnosticCode::asserted_ambiguous_numeric_value),
            "N-04d: asserting a scalar on ambiguous PDF text '1,000' fails closed at intake",
            checks);

        // Attempting to pass a non-finite / unparsed normalized parameter into vertical preparation fails closed.
        auto bad_cap_assembly = valid_pdf_assembly;
        bad_cap_assembly.component_bundles.back()
            .knowledge->normalized_parameters[1]
            .normalized_numeric_value = std::nullopt;
        const auto prep = prepare_vertical_rc_simulation(bad_cap_assembly, v_inputs);
        gate_require(
            !prep.validation.valid() && !prep.adapted.has_value() &&
                prep.validation.has_code(VerticalValidationCode::missing_or_invalid_si_value),
            "N-04e: normalized parameter without trustworthy finite SI scalar fails closed before simulation",
            checks);
    }

    // Negative Case 5: Conflicting observations with no documented resolution.
    {
        // 5a: Both 25 degC typical resistor observations approved, but conflicts vector cleared.
        auto undeclared_conflict_assembly = valid_pdf_assembly;
        auto& res_bundle = undeclared_conflict_assembly.component_bundles.front();
        auto& r3_typ = find_observation_mut(res_bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r3.typ");
        r3_typ.review_state = component::ReviewState::candidate;
        r3_typ.review_history.clear();
        const component::ReviewerIdentity reviewer{
            "eng.synthetic.pdf-gate-reviewer",
            component::ActorKind::human_engineer,
            "Synthetic Test Fixture Engineering Reviewer"};
        queue_and_decide_observation(
            r3_typ,
            "rev.pdf.res.r3.typ.conflict-test",
            reviewer,
            component::ReviewState::approved,
            "Synthetic test approving competing observation without conflict record.");
        res_bundle.knowledge->conflicts.clear();
        const auto dom_report = component::validate_domain_bundle(res_bundle);
        gate_require(
            !dom_report.valid() &&
                dom_report.has_code(component::ValidationRuleCode::undeclared_parameter_conflict),
            "N-05a: competing approved observations without ParameterConflict record fail domain validation",
            checks);

        // 5b: Conflict recorded as unresolved -> promoting or simulating fails closed.
        auto unresolved_conflict_assembly = valid_pdf_assembly;
        auto& u_res_bundle = unresolved_conflict_assembly.component_bundles.front();
        u_res_bundle.knowledge->conflicts.front().status =
            component::ConflictResolutionStatus::unresolved;
        u_res_bundle.knowledge->conflicts.front().selected_observation_id = std::nullopt;
        u_res_bundle.knowledge->conflicts.front().reviewer = std::nullopt;
        u_res_bundle.knowledge->conflicts.front().resolution_rationale = std::nullopt;
        const auto u_prep = prepare_vertical_rc_simulation(unresolved_conflict_assembly, v_inputs);
        gate_require(
            !u_prep.validation.valid() && !u_prep.adapted.has_value() &&
                u_prep.validation.has_code(VerticalValidationCode::unresolved_knowledge_conflicts),
            "N-05b: unresolved ParameterConflict in ComponentKnowledge fails closed before simulation",
            checks);

        // 5c: Attempting promote_parameter_to_knowledge on an observation involved in an unresolved conflict throws.
        bool threw_unresolved_promo = false;
        try {
            auto kn_copy = *u_res_bundle.knowledge;
            const auto& r1_typ = find_observation_const(u_res_bundle, "obs.doc.synthetic.rc.TABLE_R.p0.r1.typ");
            component::promote_parameter_to_knowledge(
                kn_copy,
                r1_typ,
                component::make_normalized_parameter(
                    component::NormalizedParameterId{"np.pdf.resistor.r1.typ.conflict"},
                    r1_typ,
                    "dc_resistance",
                    "Conflicted 25 degC resistance",
                    10000.0,
                    "ohm",
                    false,
                    "explicit_scale: 10.0 kohm * 1000 = 10000.0 ohm",
                    r1_typ.operating_conditions));
        } catch (const std::invalid_argument&) {
            threw_unresolved_promo = true;
        }
        gate_require(
            threw_unresolved_promo,
            "N-05c: promote_parameter_to_knowledge rejects observation in unresolved conflict",
            checks);
    }

    // Negative Case 6: Candidate promoted without the required review transitions.
    {
        auto candidate_bundle = *res_extraction.candidate_bundle;
        auto candidate_obs = candidate_bundle.observations.front();

        // 6a: Direct candidate -> approved transition (skipping needs_review) throws.
        bool threw_skip_needs_review = false;
        try {
            const component::ReviewerIdentity reviewer{
                "eng.synthetic.pdf-gate-reviewer",
                component::ActorKind::human_engineer,
                "Synthetic Test Fixture Engineering Reviewer"};
            component::apply_review_decision(
                candidate_obs,
                component::ReviewDecision{
                    component::ReviewDecisionId{"rev.skip.needs-review"},
                    reviewer,
                    "2026-11-05T10:00:00Z",
                    "Attempted direct candidate -> approved transition.",
                    candidate_obs.evidence_refs,
                    component::ReviewState::candidate,
                    component::ReviewState::approved,
                    std::nullopt});
        } catch (const std::invalid_argument&) {
            threw_skip_needs_review = true;
        }
        gate_require(
            threw_skip_needs_review,
            "N-06a: direct candidate -> approved transition is rejected",
            checks);

        // 6b: promote_parameter_to_knowledge on ReviewState::candidate throws.
        bool threw_candidate_promo = false;
        try {
            auto kn_copy = *valid_pdf_assembly.component_bundles.front().knowledge;
            component::promote_parameter_to_knowledge(
                kn_copy,
                candidate_obs,
                component::make_normalized_parameter(
                    component::NormalizedParameterId{"np.unreviewed.candidate"},
                    candidate_obs,
                    "dc_resistance",
                    "Unreviewed candidate resistance",
                    9900.0,
                    "ohm",
                    false,
                    "explicit_scale: 9.9 kohm * 1000 = 9900.0 ohm",
                    candidate_obs.operating_conditions));
        } catch (const std::invalid_argument&) {
            threw_candidate_promo = true;
        }
        gate_require(
            threw_candidate_promo,
            "N-06b: promote_parameter_to_knowledge rejects ReviewState::candidate observation",
            checks);

        // 6c: promote_parameter_to_knowledge on ReviewState::needs_review throws.
        component::apply_review_decision(
            candidate_obs,
            component::ReviewDecision{
                component::ReviewDecisionId{"rev.queue.only"},
                component::ReviewerIdentity{
                    "check.pdf.synthetic.gate", component::ActorKind::automated_check, std::nullopt},
                "2026-11-05T10:00:00Z",
                "Queued to needs_review only.",
                candidate_obs.evidence_refs,
                component::ReviewState::candidate,
                component::ReviewState::needs_review,
                std::nullopt});
        bool threw_needs_review_promo = false;
        try {
            auto kn_copy = *valid_pdf_assembly.component_bundles.front().knowledge;
            component::promote_parameter_to_knowledge(
                kn_copy,
                candidate_obs,
                component::make_normalized_parameter(
                    component::NormalizedParameterId{"np.unapproved.needs-review"},
                    candidate_obs,
                    "dc_resistance",
                    "Unapproved needs_review resistance",
                    9900.0,
                    "ohm",
                    false,
                    "explicit_scale: 9.9 kohm * 1000 = 9900.0 ohm",
                    candidate_obs.operating_conditions));
        } catch (const std::invalid_argument&) {
            threw_needs_review_promo = true;
        }
        gate_require(
            threw_needs_review_promo,
            "N-06c: promote_parameter_to_knowledge rejects ReviewState::needs_review observation",
            checks);

        // 6d: Mutating source observation back to candidate in an assembly fails vertical preparation.
        auto unapproved_assembly = valid_pdf_assembly;
        auto& src_obs = find_observation_mut(
            unapproved_assembly.component_bundles.front(),
            "obs.doc.synthetic.rc.TABLE_R.p0.r2.typ");
        src_obs.review_state = component::ReviewState::candidate;
        src_obs.review_history.clear();
        const auto unapproved_prep = prepare_vertical_rc_simulation(unapproved_assembly, v_inputs);
        gate_require(
            !unapproved_prep.validation.valid() && !unapproved_prep.adapted.has_value() &&
                unapproved_prep.validation.has_code(VerticalValidationCode::unapproved_source_observation),
            "N-06d: unreviewed candidate observation in assembly fails closed before simulation",
            checks);
    }

    // Negative Case 7: Approval attempted by automatic_parser or llm_assistant.
    {
        for (const auto unauthorized_kind : {
                 component::ActorKind::automatic_parser,
                 component::ActorKind::llm_assistant,
                 component::ActorKind::automated_check}) {
            auto obs = res_extraction.candidate_bundle->observations.front();
            component::apply_review_decision(
                obs,
                component::ReviewDecision{
                    component::ReviewDecisionId{"rev.unauth.queue"},
                    component::ReviewerIdentity{
                        "check.pdf.synthetic.gate", component::ActorKind::automated_check, std::nullopt},
                    "2026-11-05T10:00:00Z",
                    "Queued for review.",
                    obs.evidence_refs,
                    component::ReviewState::candidate,
                    component::ReviewState::needs_review,
                    std::nullopt});
            bool threw_unauth = false;
            try {
                component::apply_review_decision(
                    obs,
                    component::ReviewDecision{
                        component::ReviewDecisionId{"rev.unauth.approve"},
                        component::ReviewerIdentity{"actor.unauthorized", unauthorized_kind, std::nullopt},
                        "2026-11-05T10:01:00Z",
                        "Unauthorized actor attempting approval.",
                        obs.evidence_refs,
                        component::ReviewState::needs_review,
                        component::ReviewState::approved,
                        std::nullopt});
            } catch (const std::invalid_argument&) {
                threw_unauth = true;
            }
            gate_require(
                threw_unauth,
                "N-07a: apply_review_decision rejects non-human approval actor",
                checks);
        }

        // Embedding llm_assistant as ComponentKnowledge reviewer fails domain & vertical validation.
        auto llm_assembly = valid_pdf_assembly;
        llm_assembly.component_bundles.front().knowledge->review_decision.reviewer = {
            "llm.assistant.v1", component::ActorKind::llm_assistant, "Unauthorized LLM Reviewer"};
        const auto llm_dom = component::validate_domain_bundle(llm_assembly.component_bundles.front());
        const auto llm_prep = prepare_vertical_rc_simulation(llm_assembly, v_inputs);
        gate_require(
            !llm_dom.valid() &&
                llm_dom.has_code(component::ValidationRuleCode::unauthorized_reviewer_kind) &&
                !llm_prep.validation.valid() && !llm_prep.adapted.has_value(),
            "N-07b: llm_assistant knowledge reviewer fails closed",
            checks);
    }

    // Negative Case 8: Missing or incompatible model validation (and policy mismatch).
    {
        // 8a: Unvalidated model.
        auto unval_assembly = valid_pdf_assembly;
        auto& mval = unval_assembly.component_bundles.front().model_validations.front();
        mval.status = component::ModelValidationStatus::unvalidated;
        mval.validation_kind = component::ModelValidationKind::none;
        mval.reviewer = std::nullopt;
        mval.timestamp = std::nullopt;
        mval.evidence_refs.clear();
        mval.verified_analyses.clear();
        const auto unval_prep = prepare_vertical_rc_simulation(unval_assembly, v_inputs);
        gate_require(
            !unval_prep.validation.valid() && !unval_prep.adapted.has_value() &&
                unval_prep.validation.has_code(VerticalValidationCode::model_unvalidated),
            "N-08a: unvalidated model fails closed",
            checks);

        // 8b: Failed validation model.
        auto failed_val_assembly = valid_pdf_assembly;
        failed_val_assembly.component_bundles.front().model_validations.front().status =
            component::ModelValidationStatus::failed_validation;
        const auto failed_prep = prepare_vertical_rc_simulation(failed_val_assembly, v_inputs);
        gate_require(
            !failed_prep.validation.valid() && !failed_prep.adapted.has_value() &&
                failed_prep.validation.has_code(VerticalValidationCode::model_failed_validation),
            "N-08b: failed_validation model fails closed",
            checks);

        // 8c: Transient analysis not verified in validation record.
        auto no_tran_assembly = valid_pdf_assembly;
        no_tran_assembly.component_bundles.back().model_validations.front().verified_analyses = {
            component::AnalysisType::dc_operating_point};
        const auto no_tran_prep = prepare_vertical_rc_simulation(no_tran_assembly, v_inputs);
        gate_require(
            !no_tran_prep.validation.valid() && !no_tran_prep.adapted.has_value() &&
                no_tran_prep.validation.has_code(
                    VerticalValidationCode::analysis_not_verified_in_validation_record),
            "N-08c: model without verified transient analysis fails closed",
            checks);

        // 8d: Synthetic PDF-derived assembly rejected under engineering_approved_only policy.
        auto prod_inputs = v_inputs;
        prod_inputs.execution_policy = ExecutionPolicy::engineering_approved_only;
        const auto prod_prep = prepare_vertical_rc_simulation(valid_pdf_assembly, prod_inputs);
        gate_require(
            !prod_prep.validation.valid() && !prod_prep.adapted.has_value() &&
                prod_prep.validation.has_code(
                    VerticalValidationCode::synthetic_data_prohibited_in_engineering_mode),
            "N-08d: synthetic PDF assembly is blocked under engineering_approved_only policy",
            checks);
    }

    // Negative Case 9: Operating conditions outside the supported validity envelope.
    {
        // 9a: Runtime ambient temperature (150 degC) outside [-55, 125] degC envelope.
        auto hot_inputs = v_inputs;
        hot_inputs.solver.ambient_temperature_degC = 150.0;
        const auto hot_prep = prepare_vertical_rc_simulation(valid_pdf_assembly, hot_inputs);
        gate_require(
            !hot_prep.validation.valid() && !hot_prep.adapted.has_value() &&
                hot_prep.validation.has_code(
                    VerticalValidationCode::operating_condition_outside_validity_envelope),
            "N-09a: runtime ambient temperature outside validity envelope fails closed",
            checks);

        // 9b: Runtime stimulus voltage (60 V) outside capacitor [-50, 50] V and resistor 0.050 W envelope.
        auto high_v_inputs = v_inputs;
        high_v_inputs.stimulus.voltage_V = 60.0;
        const auto high_v_prep = prepare_vertical_rc_simulation(valid_pdf_assembly, high_v_inputs);
        gate_require(
            !high_v_prep.validation.valid() && !high_v_prep.adapted.has_value() &&
                high_v_prep.validation.has_code(
                    VerticalValidationCode::operating_condition_outside_validity_envelope),
            "N-09b: runtime stimulus voltage/power outside validity envelope fails closed",
            checks);

        // 9c: PDF-extracted parameter condition (T_A=85 degC) outside a narrowed model envelope ([-20, 70] degC).
        auto narrow_env_assembly = valid_pdf_assembly;
        narrow_env_assembly.component_bundles.front()
            .models.front()
            .validity_envelope.operating_ranges.front()
            .max_value = 70.0;
        const auto narrow_prep = prepare_vertical_rc_simulation(narrow_env_assembly, v_inputs);
        gate_require(
            !narrow_prep.validation.valid() && !narrow_prep.adapted.has_value() &&
                narrow_prep.validation.has_code(
                    VerticalValidationCode::operating_condition_outside_validity_envelope),
            "N-09c: PDF parameter condition T_A=85 degC outside 70 degC model envelope fails closed",
            checks);
    }

    // Negative Case 10: Missing Poppler or ngspice executable.
    {
        auto missing_pdfinfo_req = make_table_request(
            pdf_path,
            workspace_dir / "nonexistent_pdfinfo.exe",
            pdftotext_path,
            0U,
            "resistor",
            "SIM-R-10K",
            "doc.synthetic.missing-pdfinfo");
        const auto missing_pdfinfo_res = pdf::extract_electrical_table(missing_pdfinfo_req);
        gate_require(
            !missing_pdfinfo_res.candidate_bundle.has_value() &&
                !missing_pdfinfo_res.manifest.has_value() &&
                !missing_pdfinfo_res.diagnostics.empty(),
            "N-10a: missing pdfinfo executable fails closed",
            checks);

        auto missing_pdftotext_req = make_table_request(
            pdf_path,
            pdfinfo_path,
            workspace_dir / "nonexistent_pdftotext.exe",
            0U,
            "resistor",
            "SIM-R-10K",
            "doc.synthetic.missing-pdftotext");
        const auto missing_pdftotext_res = pdf::extract_electrical_table(missing_pdftotext_req);
        gate_require(
            !missing_pdftotext_res.candidate_bundle.has_value() &&
                !missing_pdftotext_res.manifest.has_value() &&
                !missing_pdftotext_res.diagnostics.empty(),
            "N-10b: missing pdftotext executable fails closed",
            checks);

        bool missing_ngspice_threw = false;
        const auto missing_ngspice_dir = workspace_dir / "missing-ngspice-run";
        try {
            (void)execute_vertical_rc_run(
                valid_pdf_assembly,
                v_inputs,
                (workspace_dir / "nonexistent_ngspice_con.exe").string(),
                missing_ngspice_dir);
        } catch (const std::exception&) {
            missing_ngspice_threw = true;
        }
        gate_require(
            missing_ngspice_threw && !fs::exists(missing_ngspice_dir / "results.csv"),
            "N-10c: missing ngspice executable fails closed without simulation results",
            checks);
    }
}

} // namespace

CircuitAssembly make_pdf_extracted_rc_assembly(
    const fs::path& pdf_path,
    const fs::path& pdfinfo_path,
    const fs::path& pdftotext_path) {
    std::size_t checks = 0;
    const auto res_req = make_table_request(
        pdf_path, pdfinfo_path, pdftotext_path, 0U, "resistor", "SIM-R-10K", "doc.synthetic.rc");
    const auto cap_req = make_table_request(
        pdf_path, pdfinfo_path, pdftotext_path, 1U, "capacitor", "SIM-C-1UF", "doc.synthetic.cap");
    const auto res_ext = pdf::extract_electrical_table(res_req);
    const auto cap_ext = pdf::extract_electrical_table(cap_req);
    auto res_bundle = build_reviewed_resistor_bundle_from_pdf(res_ext, checks);
    auto cap_bundle = build_reviewed_capacitor_bundle_from_pdf(cap_ext, checks);
    return assemble_pdf_rc_circuit(std::move(res_bundle), std::move(cap_bundle));
}

void run_pdf_to_simulation_gate(
    const fs::path& ngspice_exe,
    const fs::path& workspace_dir) {
    std::size_t checks = 0;

    const auto pdfinfo_exe = find_executable("pdfinfo");
    const auto pdftotext_exe = find_executable("pdftotext");
    gate_require(
        !pdfinfo_exe.empty() && !pdftotext_exe.empty(),
        "Poppler pdfinfo and pdftotext executables must be available on PATH",
        checks);

    const fs::path pdf_path{DATASIM_SYNTHETIC_PDF_PATH};
    const fs::path image_only_pdf_path{DATASIM_IMAGE_ONLY_PDF_PATH};
    gate_require(fs::exists(pdf_path), "synthetic_datasheet.pdf fixture exists", checks);
    gate_require(fs::exists(image_only_pdf_path), "image_only_datasheet.pdf fixture exists", checks);

    // Step 4(1)-(2): Actual Poppler extraction on Page 0 (resistor) and Page 1 (capacitor).
    const auto res_req = make_table_request(
        pdf_path, pdfinfo_exe, pdftotext_exe, 0U, "resistor", "SIM-R-10K", "doc.synthetic.rc");
    const auto cap_req = make_table_request(
        pdf_path, pdfinfo_exe, pdftotext_exe, 1U, "capacitor", "SIM-C-1UF", "doc.synthetic.cap");

    const auto res_ext = pdf::extract_electrical_table(res_req);
    const auto cap_ext = pdf::extract_electrical_table(cap_req);

    // Step 5(11): Repeat extraction determinism check across both pages.
    const auto res_ext_repeat = pdf::extract_electrical_table(res_req);
    const auto cap_ext_repeat = pdf::extract_electrical_table(cap_req);
    gate_require(
        res_ext_repeat.candidate_bundle == res_ext.candidate_bundle &&
            res_ext_repeat.diagnostics == res_ext.diagnostics &&
            res_ext_repeat.report.diagnostics == res_ext.report.diagnostics &&
            res_ext_repeat.report.domain_validation == res_ext.report.domain_validation,
        "repeat resistor PDF extraction produces identical candidate bundle, ordering, and diagnostics",
        checks);
    gate_require(
        cap_ext_repeat.candidate_bundle == cap_ext.candidate_bundle &&
            cap_ext_repeat.diagnostics == cap_ext.diagnostics &&
            cap_ext_repeat.report.diagnostics == cap_ext.report.diagnostics &&
            cap_ext_repeat.report.domain_validation == cap_ext.report.domain_validation,
        "repeat capacitor PDF extraction produces identical candidate bundle, ordering, and diagnostics",
        checks);

    // Step 4(3)-(10): Intake validation, candidate verification, synthetic human review,
    // conflict preservation, explicit SI normalization, knowledge promotion, model binding, and assembly.
    auto res_bundle = build_reviewed_resistor_bundle_from_pdf(res_ext, checks);
    auto cap_bundle = build_reviewed_capacitor_bundle_from_pdf(cap_ext, checks);
    const auto pdf_assembly = assemble_pdf_rc_circuit(std::move(res_bundle), std::move(cap_bundle));

    const auto assembly_report = validate_circuit_assembly(pdf_assembly);
    gate_require(assembly_report.valid(), "PDF-derived CircuitAssembly validates cleanly", checks);

    const auto repeat_assembly = make_pdf_extracted_rc_assembly(pdf_path, pdfinfo_exe, pdftotext_exe);
    gate_require(
        repeat_assembly == pdf_assembly,
        "repeat PDF-to-CircuitAssembly construction is deterministic and equal",
        checks);

    // Step 4(11): Execute through datasim_vertical against real ngspice.
    const auto v_inputs = make_synthetic_test_run_inputs();
    const auto pdf_run_dir = workspace_dir / "pdf-vertical-run";
    const auto pdf_repeat_dir = workspace_dir / "pdf-vertical-repeat-run";
    const auto manual_10k_dir = workspace_dir / "manual-10k-run";

    const auto pdf_run = execute_vertical_rc_run(
        pdf_assembly, v_inputs, ngspice_exe.string(), pdf_run_dir);
    const auto pdf_repeat_run = execute_vertical_rc_run(
        repeat_assembly, v_inputs, ngspice_exe.string(), pdf_repeat_dir);

    gate_require(
        pdf_run.run.validation.passed && pdf_run.run.validation.errors.size() == 4001U,
        "PDF-derived vertical RC simulation passed common-grid validation",
        checks);
    gate_require(
        pdf_run.adapted.rc_config.resistance_ohm == 10000.0 &&
            pdf_run.adapted.rc_config.capacitance_F == 1.0e-6,
        "PDF-derived adapted RcConfig resolved R=10000 ohm and C=1.0e-6 F",
        checks);

    // Step 4(12)-(13): Compare against equivalent manually configured RcConfig (R=10000 ohm, C=1.0e-6 F).
    RcConfig manual_10k_cfg{};
    manual_10k_cfg.resistance_ohm = 10000.0;
    manual_10k_cfg.capacitance_F = 1.0e-6;
    const auto manual_10k_run = execute_rc_run(
        manual_10k_cfg, ngspice_exe.string(), manual_10k_dir);
    gate_require(
        manual_10k_run.validation.passed && manual_10k_run.validation.errors.size() == 4001U,
        "equivalent manual 10k/1uF RC run passed validation",
        checks);
    gate_require(
        pdf_run.run.samples.size() == manual_10k_run.samples.size(),
        "PDF-derived sample count matches equivalent manual RC run",
        checks);

    double max_sample_diff_V = 0.0;
    for (std::size_t i = 0; i < manual_10k_run.samples.size(); ++i) {
        gate_require(
            pdf_run.run.samples[i].time_s == manual_10k_run.samples[i].time_s &&
                pdf_run.run.samples[i].vin_V == manual_10k_run.samples[i].vin_V,
            "PDF-derived time/input sample matches manual RC run",
            checks);
        max_sample_diff_V = std::max(
            max_sample_diff_V,
            std::abs(pdf_run.run.samples[i].vout_V - manual_10k_run.samples[i].vout_V));
    }
    gate_require(
        max_sample_diff_V == 0.0 && pdf_run.run.validation.max_absolute_error_V <= 0.05,
        "PDF-derived waveform matches equivalent manual RC run with 0.0 V difference and <= 0.05 V analytical error",
        checks);

    gate_require(
        read_bytes(pdf_run_dir / "results.csv") == read_bytes(manual_10k_dir / "results.csv") &&
            read_bytes(pdf_run_dir / "errors.csv") == read_bytes(manual_10k_dir / "errors.csv") &&
            read_bytes(pdf_run_dir / "circuit.cir") == read_bytes(manual_10k_dir / "circuit.cir") &&
            read_bytes(pdf_run_dir / "validation_report.json") ==
                read_bytes(manual_10k_dir / "validation_report.json"),
        "PDF-derived results.csv, errors.csv, circuit.cir, and validation_report.json are byte-identical to manual RC run",
        checks);

    // Repeat end-to-end run determinism.
    gate_require(
        pdf_repeat_run.provenance == pdf_run.provenance &&
            read_bytes(pdf_repeat_dir / "results.csv") == read_bytes(pdf_run_dir / "results.csv") &&
            read_bytes(pdf_repeat_dir / "provenance_metadata.json") ==
                read_bytes(pdf_run_dir / "provenance_metadata.json"),
        "repeat PDF-to-Simulation execution produces byte-identical artifacts and provenance",
        checks);

    // Step 4(14): Verify provenance identifies selected synthetic evidence, reviewed knowledge, model, and classification.
    const auto& prov = pdf_run.provenance;
    gate_require(
        prov.execution_classification == ExecutionClassification::test_only_synthetic &&
            prov.is_synthetic_test_run &&
            prov.execution_status == "SUCCEEDED" &&
            prov.validation_status == "PASSED",
        "provenance manifest records test_only_synthetic classification and SUCCEEDED/PASSED status",
        checks);
    gate_require(
        prov.resistor.component_id.value == "cmp.fixture.resistor" &&
            prov.resistor.part_number == "SIM-R-10K" &&
            prov.resistor.is_synthetic &&
            prov.resistor.knowledge_id.value == "ck.pdf.synthetic.resistor.v1" &&
            prov.resistor.knowledge_review_decision_id.value == "rev.pdf.res.knowledge.v1" &&
            prov.resistor.knowledge_reviewer_id == "eng.synthetic.pdf-gate-reviewer" &&
            prov.resistor.model_id.value == "model.synthetic.pdf.resistor.ohmic-linear" &&
            prov.resistor.model_validation_status ==
                component::ModelValidationStatus::validated_within_envelope &&
            prov.resistor.normalized_parameter_id.value == "np.pdf.resistor.r_dc.typ" &&
            prov.resistor.source_observation_id.value ==
                "obs.doc.synthetic.rc.TABLE_R.p0.r2.typ" &&
            prov.resistor.preserved_original_value_text == "10.0" &&
            prov.resistor.preserved_original_unit_symbol == "kohm" &&
            prov.resistor.resolved_si_value == 10000.0 &&
            prov.resistor.normalized_si_unit == "ohm" &&
            prov.resistor.source_document_ids.size() == 1U &&
            prov.resistor.source_document_ids.front().value == "doc.synthetic.rc" &&
            prov.resistor.evidence_location_ids.size() == 2U &&
            prov.resistor.evidence_location_ids.back().value ==
                "ev.doc.synthetic.rc.TABLE_R.p0.r2.typ",
        "provenance manifest records complete PDF-to-knowledge-to-model lineage for resistor",
        checks);
    gate_require(
        prov.capacitor.component_id.value == "cmp.fixture.capacitor" &&
            prov.capacitor.part_number == "SIM-C-1UF" &&
            prov.capacitor.is_synthetic &&
            prov.capacitor.knowledge_id.value == "ck.pdf.synthetic.capacitor.v1" &&
            prov.capacitor.knowledge_review_decision_id.value == "rev.pdf.cap.knowledge.v1" &&
            prov.capacitor.knowledge_reviewer_id == "eng.synthetic.pdf-gate-reviewer" &&
            prov.capacitor.model_id.value == "model.synthetic.pdf.capacitor.ideal-linear" &&
            prov.capacitor.model_validation_status ==
                component::ModelValidationStatus::validated_within_envelope &&
            prov.capacitor.normalized_parameter_id.value == "np.pdf.capacitor.c.typ" &&
            prov.capacitor.source_observation_id.value ==
                "obs.doc.synthetic.cap.TABLE_C.p1.r3.typ" &&
            prov.capacitor.preserved_original_value_text == "1.0" &&
            prov.capacitor.preserved_original_unit_symbol == "uF" &&
            prov.capacitor.resolved_si_value == 1.0e-6 &&
            prov.capacitor.normalized_si_unit == "F" &&
            prov.capacitor.source_document_ids.size() == 1U &&
            prov.capacitor.source_document_ids.front().value == "doc.synthetic.cap" &&
            prov.capacitor.evidence_location_ids.size() == 2U &&
            prov.capacitor.evidence_location_ids.back().value ==
                "ev.doc.synthetic.cap.TABLE_C.p1.r3.typ",
        "provenance manifest records complete PDF-to-knowledge-to-model lineage for capacitor",
        checks);

    const auto prov_json = read_bytes(pdf_run_dir / "provenance_metadata.json");
    gate_require(
        prov_json.find("\"execution_classification\": \"test_only_synthetic\"") != std::string::npos ||
            prov_json.find("\"execution_classification\":\"test_only_synthetic\"") != std::string::npos,
        "provenance_metadata.json serializes test_only_synthetic classification",
        checks);
    gate_require(
        prov_json.find("obs.doc.synthetic.rc.TABLE_R.p0.r2.typ") != std::string::npos &&
            prov_json.find("obs.doc.synthetic.cap.TABLE_C.p1.r3.typ") != std::string::npos &&
            prov_json.find("ck.pdf.synthetic.resistor.v1") != std::string::npos &&
            prov_json.find("ck.pdf.synthetic.capacitor.v1") != std::string::npos &&
            prov_json.find("model.synthetic.pdf.resistor.ohmic-linear") != std::string::npos &&
            prov_json.find("model.synthetic.pdf.capacitor.ideal-linear") != std::string::npos,
        "provenance_metadata.json serializes PDF evidence, knowledge, and model identifiers",
        checks);

    // Step 5: Execute all negative and fail-closed trust-boundary tests.
    run_negative_and_trust_boundary_tests(
        pdf_path,
        image_only_pdf_path,
        pdfinfo_exe,
        pdftotext_exe,
        res_ext,
        cap_ext,
        pdf_assembly,
        workspace_dir,
        checks);

    std::cout << "PASS: Phase 4D PDF-to-Simulation gate (" << checks
              << " checks, R=10000 ohm, C=1e-6 F, samples=" << pdf_run.run.samples.size()
              << ", max_error_V=" << pdf_run.run.validation.max_absolute_error_V
              << ", rmse_V=" << pdf_run.run.validation.rmse_V
              << ", manual_diff_V=" << max_sample_diff_V << ")\n";
}

} // namespace datasim::vertical::testing

