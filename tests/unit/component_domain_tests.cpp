#include "component_fixtures.hpp"
#include "datasim/component/evidence/evidence_location.hpp"
#include "datasim/component/evidence/source_document.hpp"
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/model/model_artifact.hpp"
#include "datasim/component/model/model_validation.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/component/observation/review.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

using namespace datasim::component;
using namespace datasim::component::testing;

namespace {

int checks = 0;

void require(bool condition, const char* context = "") {
    ++checks;
    if (!condition) {
        throw std::runtime_error(
            std::string("component check failed #") + std::to_string(checks) +
            (context[0] != '\0' ? std::string(": ") + context : std::string{}));
    }
}

void rejects(const std::function<void()>& fn, const char* context = "") {
    ++checks;
    bool threw = false;
    try {
        fn();
    } catch (const std::exception&) {
        threw = true;
    }
    if (!threw) {
        throw std::runtime_error(
            std::string("expected exception not thrown #") + std::to_string(checks) +
            (context[0] != '\0' ? std::string(": ") + context : std::string{}));
    }
}

void require_invalid_with_code(
    const ValidationReport& report,
    ValidationRuleCode expected,
    const char* context = "") {
    require(!report.valid(), context);
    require(report.has_code(expected), context);
}

void test_synthetic_fixtures_baseline() {
    const auto res_bundle = make_synthetic_resistor_bundle();
    require(res_bundle.component.is_synthetic, "resistor marked synthetic");
    require(res_bundle.documents.front().is_synthetic, "resistor doc marked synthetic");
    require(res_bundle.models.front().is_synthetic, "resistor model marked synthetic");
    const auto res_report = validate_domain_bundle(res_bundle);
    require(res_report.valid(), "synthetic resistor bundle valid");
    ensure_valid(res_report);

    const auto cap_bundle = make_synthetic_capacitor_bundle();
    require(cap_bundle.component.is_synthetic, "capacitor marked synthetic");
    require(cap_bundle.documents.front().is_synthetic, "capacitor doc marked synthetic");
    require(cap_bundle.models.front().is_synthetic, "capacitor model marked synthetic");
    const auto cap_report = validate_domain_bundle(cap_bundle);
    require(cap_report.valid(), "synthetic capacitor bundle valid");
    ensure_valid(cap_report);
}

void test_component_identity_rules() {
    const auto valid_res = make_synthetic_resistor_bundle().component;
    require(validate_component_identity(valid_res).valid(), "valid resistor identity");

    // Exact manufacturer, part number, and suffix preservation (no stripping/case-folding).
    ComponentIdentity suffix_part;
    suffix_part.id = ComponentId{"comp.synthetic.res.rc0603fr-071kl-nopb"};
    suffix_part.manufacturer = "Synthetic Passive Mfg Co., Ltd.";
    suffix_part.part_number = "RC0603FR-071KL/NOPB";
    suffix_part.part_number_suffix = "FR-071KL/NOPB";
    suffix_part.package_id = "0603 (1608 Metric)";
    suffix_part.family = ComponentFamily::resistor;
    suffix_part.is_synthetic = true;
    require(validate_component_identity(suffix_part).valid(), "exact suffix identity valid");
    require(suffix_part.part_number == "RC0603FR-071KL/NOPB", "part number verbatim");
    require(suffix_part.part_number_suffix == "FR-071KL/NOPB", "part suffix verbatim");

    // Same part number across two distinct manufacturers produces distinct identities.
    ComponentIdentity other_mfg = suffix_part;
    other_mfg.id = ComponentId{"comp.synthetic.res.second-source-071kl"};
    other_mfg.manufacturer = "Second Source Synthetic Passive Inc.";
    require(other_mfg != suffix_part, "manufacturer distinguishes identical MPN");

    // Missing manufacturer.
    for (const char* bad_mfg : {"", "   ", "Bad\nMfg"}) {
        auto broken = valid_res;
        broken.manufacturer = bad_mfg;
        require_invalid_with_code(
            validate_component_identity(broken),
            ValidationRuleCode::empty_required_field,
            "missing or control-char manufacturer rejected");
    }

    // Missing part number.
    for (const char* bad_mpn : {"", "   ", "SYN-1K\tBAD"}) {
        auto broken = valid_res;
        broken.part_number = bad_mpn;
        require_invalid_with_code(
            validate_component_identity(broken),
            ValidationRuleCode::empty_required_field,
            "missing or control-char part number rejected");
    }

    // Missing or malformed ComponentId.
    {
        auto broken = valid_res;
        broken.id = ComponentId{""};
        require_invalid_with_code(
            validate_component_identity(broken),
            ValidationRuleCode::empty_required_field,
            "empty ComponentId rejected");
    }
    {
        auto broken = valid_res;
        broken.id = ComponentId{"comp with spaces"};
        require_invalid_with_code(
            validate_component_identity(broken),
            ValidationRuleCode::invalid_identifier,
            "whitespace ComponentId rejected");
    }

    // Blank optional suffix/package strings rejected (must use std::nullopt when absent).
    {
        auto broken = valid_res;
        broken.part_number_suffix = "   ";
        require_invalid_with_code(
            validate_component_identity(broken),
            ValidationRuleCode::empty_required_field,
            "blank present suffix rejected");
    }
}

void test_evidence_and_source_document_rules() {
    auto bundle = make_synthetic_resistor_bundle();
    const auto& doc = bundle.documents.front();
    require(validate_source_document(doc).valid(), "valid source document");

    // PDF 0-based page index is independent of printed page label; figure locator valid.
    EvidenceLocation fig_loc;
    fig_loc.id = EvidenceId{"ev.res.fig1.derating"};
    fig_loc.source_document_id = doc.id;
    fig_loc.pdf_page_index = 0U;
    fig_loc.printed_page_label = "iii";
    fig_loc.section = "Front Matter / Derating Overview";
    fig_loc.table_or_figure_id = "Figure 1";
    fig_loc.cell_or_region = "Curve A (0603 package)";
    fig_loc.footnote_refs = {"Footnote *: Free-air FR4 test board"};
    require(validate_evidence_location(fig_loc).valid(), "valid page and figure location");
    require(*fig_loc.pdf_page_index == 0U && *fig_loc.printed_page_label == "iii",
            "pdf_page_index distinct from printed_page_label");

    // Missing source document reference on EvidenceLocation.
    {
        auto broken = fig_loc;
        broken.source_document_id = SourceDocumentId{""};
        require_invalid_with_code(
            validate_evidence_location(broken),
            ValidationRuleCode::empty_required_field,
            "missing source_document_id rejected");
    }

    // Empty evidence locator (no page, section, table/figure, or region).
    {
        EvidenceLocation empty_loc;
        empty_loc.id = EvidenceId{"ev.empty"};
        empty_loc.source_document_id = doc.id;
        require_invalid_with_code(
            validate_evidence_location(empty_loc),
            ValidationRuleCode::empty_evidence_locator,
            "empty locator rejected");
    }

    // Invalid evidence reference: EvidenceLocation -> non-existent SourceDocumentId.
    {
        auto bad_bundle = bundle;
        bad_bundle.evidence_locations.front().source_document_id =
            SourceDocumentId{"doc.does.not.exist"};
        require_invalid_with_code(
            validate_domain_bundle(bad_bundle),
            ValidationRuleCode::unresolved_reference,
            "dangling source_document_id rejected");
    }

    // Invalid evidence reference: ParameterObservation -> non-existent EvidenceId.
    {
        auto bad_bundle = bundle;
        bad_bundle.observations.front().evidence_refs = {EvidenceId{"ev.does.not.exist"}};
        require_invalid_with_code(
            validate_domain_bundle(bad_bundle),
            ValidationRuleCode::unresolved_reference,
            "dangling observation EvidenceId rejected");
    }

    // Invalid SHA-256 digest and invalid ISO-8601 retrieval date on SourceDocument.
    {
        auto bad_doc = doc;
        bad_doc.content_sha256 = "not_a_64_char_hex_sha256";
        require_invalid_with_code(
            validate_source_document(bad_doc),
            ValidationRuleCode::invalid_sha256_digest,
            "invalid document SHA-256 rejected");
    }
    {
        auto bad_doc = doc;
        bad_doc.retrieval_date = "2026-02-30";
        require_invalid_with_code(
            validate_source_document(bad_doc),
            ValidationRuleCode::invalid_date_or_timestamp,
            "impossible calendar date rejected");
    }
}

void test_parameter_observation_rules() {
    const auto cap_bundle = make_synthetic_capacitor_bundle();

    // Verify Minimum, Typical, Maximum, and Unspecified remain distinct qualifiers.
    ParameterObservation base_obs;
    base_obs.id = ObservationId{"obs.qual.check"};
    base_obs.component_id = cap_bundle.component.id;
    base_obs.parameter_name = "ESR";
    base_obs.original_value_text = "50";
    base_obs.original_unit_symbol = "mohm";
    base_obs.parsed_numeric_value = 50.0;
    base_obs.category = ParameterCategory::electrical_characteristic;
    base_obs.operating_conditions = {
        OperatingCondition{"f_test", "100", "kHz", 100.0, std::nullopt}};
    base_obs.evidence_refs = {cap_bundle.evidence_locations.front().id};
    base_obs.extraction_method = ExtractionMethod::synthetic_fixture;

    for (ValueQualifier q : {
             ValueQualifier::minimum,
             ValueQualifier::typical,
             ValueQualifier::maximum,
             ValueQualifier::unspecified}) {
        auto qobs = base_obs;
        qobs.qualifier = q;
        require(validate_parameter_observation(qobs).valid(), "qualifier valid and distinct");
        require(qobs.qualifier == q, "qualifier preserved");
    }

    // Missing value cell stays missing (std::nullopt), never converted to 0.0.
    {
        ParameterObservation missing_obs = base_obs;
        missing_obs.id = ObservationId{"obs.missing.blank"};
        missing_obs.original_value_text = std::nullopt;
        missing_obs.parsed_numeric_value = std::nullopt;
        require(validate_parameter_observation(missing_obs).valid(), "missing value valid");
        require(!missing_obs.parsed_numeric_value.has_value(), "missing numeric value is not zero");

        // Parsed numeric value without original_value_text is rejected.
        missing_obs.parsed_numeric_value = 0.0;
        require_invalid_with_code(
            validate_parameter_observation(missing_obs),
            ValidationRuleCode::missing_numeric_source_text,
            "coerced zero without source text rejected");
    }

    // Original notation preservation (inequalities, tolerances, ranges, symbols).
    for (const char* notation : {"<= 0.0015", "+/-1%", "10 to 100", "> 10^10", "1.000"}) {
        ParameterObservation sym = base_obs;
        sym.original_value_text = notation;
        sym.parsed_numeric_value = std::nullopt;
        require(validate_parameter_observation(sym).valid(), "symbolic notation valid");
        require(*sym.original_value_text == notation, "original notation preserved verbatim");
    }

    // Invalid NaN and +/-Infinity rejected on observation and operating condition.
    for (double non_finite : {
             std::numeric_limits<double>::quiet_NaN(),
             std::numeric_limits<double>::infinity(),
             -std::numeric_limits<double>::infinity()}) {
        auto bad_obs = base_obs;
        bad_obs.parsed_numeric_value = non_finite;
        require_invalid_with_code(
            validate_parameter_observation(bad_obs),
            ValidationRuleCode::non_finite_numeric_value,
            "non-finite observation value rejected");

        auto bad_cond_obs = base_obs;
        bad_cond_obs.operating_conditions.front().numeric_value = non_finite;
        require_invalid_with_code(
            validate_parameter_observation(bad_cond_obs),
            ValidationRuleCode::non_finite_numeric_value,
            "non-finite operating condition value rejected");
    }

    // Numeric value requiring a unit must have an explicit unit.
    {
        auto no_unit = base_obs;
        no_unit.original_unit_symbol = std::nullopt;
        no_unit.unitless = false;
        require_invalid_with_code(
            validate_parameter_observation(no_unit),
            ValidationRuleCode::missing_required_unit,
            "missing observation unit rejected");

        auto contradiction = base_obs;
        contradiction.unitless = true; // still has "mohm"
        require_invalid_with_code(
            validate_parameter_observation(contradiction),
            ValidationRuleCode::contradictory_unitless_flag,
            "unitless with unit symbol rejected");
    }

    // Operating conditions must retain their units.
    {
        auto bad_cond = base_obs;
        bad_cond.operating_conditions.front().unit_symbol = "";
        require_invalid_with_code(
            validate_parameter_observation(bad_cond),
            ValidationRuleCode::missing_required_unit,
            "operating condition missing unit rejected");
    }

    // Rating categories preserved explicitly and never inferred from parameter_name.
    for (ParameterCategory cat : {
             ParameterCategory::absolute_maximum_rating,
             ParameterCategory::recommended_operating_condition,
             ParameterCategory::electrical_characteristic,
             ParameterCategory::thermal_characteristic,
             ParameterCategory::other_unspecified}) {
        auto cobs = base_obs;
        cobs.parameter_name = "V_MAX";
        cobs.category = cat;
        require(validate_parameter_observation(cobs).valid(), "rating category valid");
        require(cobs.category == cat, "category never overwritten from parameter_name");
    }
}

void test_review_lifecycle_rules() {
    const std::array<ReviewState, 5> all_states = {
        ReviewState::candidate,
        ReviewState::needs_review,
        ReviewState::approved,
        ReviewState::rejected,
        ReviewState::superseded};

    // Exhaustive 5x5 transition matrix verification.
    for (ReviewState from : all_states) {
        for (ReviewState to : all_states) {
            const bool expected =
                (from == ReviewState::candidate && to == ReviewState::needs_review) ||
                (from == ReviewState::needs_review && to == ReviewState::approved) ||
                (from == ReviewState::needs_review && to == ReviewState::rejected) ||
                (from == ReviewState::approved && to == ReviewState::superseded);
            require(
                is_valid_review_transition(from, to) == expected,
                "deterministic review transition table");
        }
    }

    const EvidenceId ev_id{"ev.lifecycle.1"};
    const ReviewerIdentity engineer{"eng.reviewer.alpha", ActorKind::human_engineer, "Reviewer Alpha"};

    auto make_candidate = [&]() {
        ParameterObservation obs;
        obs.id = ObservationId{"obs.lifecycle.1"};
        obs.component_id = ComponentId{"comp.lifecycle.1"};
        obs.parameter_name = "R";
        obs.original_value_text = "100";
        obs.original_unit_symbol = "ohm";
        obs.parsed_numeric_value = 100.0;
        obs.qualifier = ValueQualifier::typical;
        obs.category = ParameterCategory::electrical_characteristic;
        obs.evidence_refs = {ev_id};
        obs.extraction_method = ExtractionMethod::llm_assisted;
        obs.extraction_tool = "local-llm-extractor-v0";
        return obs;
    };

    // Valid transition sequence: candidate -> needs_review -> approved -> superseded.
    {
        auto obs = make_candidate();
        apply_review_decision(
            obs,
            ReviewDecision{
                ReviewDecisionId{"dec.1"},
                ReviewerIdentity{"check.intake", ActorKind::automated_check, std::nullopt},
                "2026-10-31T12:00:00Z",
                "Structured fields complete.",
                {ev_id},
                ReviewState::candidate,
                ReviewState::needs_review,
                std::nullopt});
        require(obs.review_state == ReviewState::needs_review, "moved to needs_review");

        // Invalid transition from needs_review -> superseded rejected without mutating obs.
        rejects([&] {
            apply_review_decision(
                obs,
                ReviewDecision{
                    ReviewDecisionId{"dec.bad"},
                    engineer,
                    "2026-10-31T12:01:00Z",
                    "Invalid jump.",
                    {ev_id},
                    ReviewState::needs_review,
                    ReviewState::superseded,
                    ObservationId{"obs.replacement"}});
        }, "needs_review -> superseded rejected");
        require(obs.review_state == ReviewState::needs_review && obs.review_history.size() == 1U,
                "failed transition leaves observation unchanged");

        apply_review_decision(
            obs,
            ReviewDecision{
                ReviewDecisionId{"dec.2"},
                engineer,
                "2026-10-31T12:05:00Z",
                "Confirmed against synthetic evidence.",
                {ev_id},
                ReviewState::needs_review,
                ReviewState::approved,
                std::nullopt});
        require(obs.review_state == ReviewState::approved, "moved to approved");

        // Superseded without replacement reference rejected.
        ReviewDecision missing_rep{
            ReviewDecisionId{"dec.3.missing"},
            engineer,
            "2026-10-31T12:10:00Z",
            "Missing replacement.",
            {ev_id},
            ReviewState::approved,
            ReviewState::superseded,
            std::nullopt};
        require_invalid_with_code(
            validate_review_decision(missing_rep),
            ValidationRuleCode::missing_superseding_reference,
            "superseded without replacement rejected");
        rejects([&] { apply_review_decision(obs, missing_rep); });

        // Self-supersession rejected.
        ReviewDecision self_rep = missing_rep;
        self_rep.superseding_observation_id = obs.id;
        rejects([&] { apply_review_decision(obs, self_rep); }, "self-supersession rejected");

        // Valid supersession with distinct replacement.
        apply_review_decision(
            obs,
            ReviewDecision{
                ReviewDecisionId{"dec.3"},
                engineer,
                "2026-10-31T12:10:00Z",
                "Replaced by corrected Table 2 reading.",
                {ev_id},
                ReviewState::approved,
                ReviewState::superseded,
                ObservationId{"obs.lifecycle.2"}});
        require(obs.review_state == ReviewState::superseded, "moved to superseded");
        require(obs.superseded_by == ObservationId{"obs.lifecycle.2"}, "replacement recorded");
    }

    // Direct candidate -> approved skipping needs_review rejected.
    {
        auto obs = make_candidate();
        ReviewDecision skip{
            ReviewDecisionId{"dec.skip"},
            engineer,
            "2026-10-31T12:00:00Z",
            "Attempt to skip needs_review.",
            {ev_id},
            ReviewState::candidate,
            ReviewState::approved,
            std::nullopt};
        require_invalid_with_code(
            validate_review_decision(skip),
            ValidationRuleCode::invalid_review_transition,
            "candidate -> approved direct jump rejected");
        rejects([&] { apply_review_decision(obs, skip); });
    }

    // Missing reviewer, missing rationale, approval without evidence.
    {
        ReviewDecision base_approve{
            ReviewDecisionId{"dec.approve"},
            engineer,
            "2026-10-31T12:05:00Z",
            "Reviewed.",
            {ev_id},
            ReviewState::needs_review,
            ReviewState::approved,
            std::nullopt};

        auto no_reviewer = base_approve;
        no_reviewer.reviewer.id = "";
        require_invalid_with_code(
            validate_review_decision(no_reviewer),
            ValidationRuleCode::empty_required_field,
            "missing reviewer rejected");

        auto no_rationale = base_approve;
        no_rationale.rationale = "   ";
        require_invalid_with_code(
            validate_review_decision(no_rationale),
            ValidationRuleCode::empty_required_field,
            "missing rationale rejected");

        auto no_evidence = base_approve;
        no_evidence.evidence_refs.clear();
        require_invalid_with_code(
            validate_review_decision(no_evidence),
            ValidationRuleCode::missing_evidence_reference,
            "approval without evidence rejected");
    }

    // LLM, automatic parser, automated_check, and self-matching tool ID cannot approve.
    {
        auto obs = make_candidate();
        apply_review_decision(
            obs,
            ReviewDecision{
                ReviewDecisionId{"dec.intake"},
                engineer,
                "2026-10-31T12:00:00Z",
                "Queued for review.",
                {ev_id},
                ReviewState::candidate,
                ReviewState::needs_review,
                std::nullopt});

        for (ActorKind forbidden_kind : {
                 ActorKind::llm_assistant,
                 ActorKind::automatic_parser,
                 ActorKind::automated_check}) {
            ReviewDecision bad_actor{
                ReviewDecisionId{"dec.auto.approve"},
                ReviewerIdentity{"actor.bot", forbidden_kind, std::nullopt},
                "2026-10-31T12:05:00Z",
                "Attempted non-human approval.",
                {ev_id},
                ReviewState::needs_review,
                ReviewState::approved,
                std::nullopt};
            require_invalid_with_code(
                validate_review_decision(bad_actor),
                ValidationRuleCode::unauthorized_reviewer_kind,
                "non-human approval rejected");
            rejects([&] { apply_review_decision(obs, bad_actor); });
        }

        // Extraction tool ID masquerading as human reviewer ID rejected as self-approval.
        ReviewDecision spoof_self{
            ReviewDecisionId{"dec.self.approve"},
            ReviewerIdentity{"local-llm-extractor-v0", ActorKind::human_engineer, std::nullopt},
            "2026-10-31T12:05:00Z",
            "Attempted self-approval using extraction tool id.",
            {ev_id},
            ReviewState::needs_review,
            ReviewState::approved,
            std::nullopt};
        rejects([&] { apply_review_decision(obs, spoof_self); }, "self-approval rejected");

        // Missing-value observation cannot be approved even by a human engineer.
        obs.original_value_text = std::nullopt;
        obs.parsed_numeric_value = std::nullopt;
        ReviewDecision human_approve_blank{
            ReviewDecisionId{"dec.blank.approve"},
            engineer,
            "2026-10-31T12:06:00Z",
            "Attempt to approve blank cell.",
            {ev_id},
            ReviewState::needs_review,
            ReviewState::approved,
            std::nullopt};
        rejects([&] { apply_review_decision(obs, human_approve_blank); },
                "blank observation cannot be approved");
    }
}

void test_component_knowledge_rules() {
    // 1. Rejected observation exclusion.
    {
        auto bundle = make_synthetic_resistor_bundle();
        ParameterObservation* rejected_obs = nullptr;
        for (auto& o : bundle.observations) {
            if (o.review_state == ReviewState::rejected) {
                rejected_obs = &o;
                break;
            }
        }
        require(rejected_obs != nullptr, "fixture contains rejected observation");

        rejects([&] {
            (void)make_normalized_parameter(
                NormalizedParameterId{"np.bad.rejected"},
                *rejected_obs,
                "dc_resistance",
                "Rejected observation",
                10000.0,
                "ohm",
                false,
                "explicit_scale: 10 kohm * 1000",
                rejected_obs->operating_conditions);
        }, "make_normalized_parameter rejects rejected observation");

        bundle.knowledge->approved_observation_refs.push_back(rejected_obs->id);
        require_invalid_with_code(
            validate_domain_bundle(bundle),
            ValidationRuleCode::unapproved_observation_in_knowledge,
            "rejected observation excluded from approved_observation_refs");
    }

    // 2. Unresolved (candidate / needs_review) and superseded observation exclusion.
    {
        auto cap_bundle = make_synthetic_capacitor_bundle();
        ObservationId superseded_id;
        ObservationId candidate_id;
        for (const auto& o : cap_bundle.observations) {
            if (o.review_state == ReviewState::superseded) {
                superseded_id = o.id;
            } else if (o.review_state == ReviewState::candidate) {
                candidate_id = o.id;
            }
        }
        require(!superseded_id.empty() && !candidate_id.empty(),
                "capacitor fixture has superseded and candidate observations");

        auto bad_sup = cap_bundle;
        bad_sup.knowledge->approved_observation_refs.push_back(superseded_id);
        require_invalid_with_code(
            validate_domain_bundle(bad_sup),
            ValidationRuleCode::unapproved_observation_in_knowledge,
            "superseded observation excluded from knowledge");

        auto bad_cand = cap_bundle;
        bad_cand.knowledge->approved_observation_refs.push_back(candidate_id);
        require_invalid_with_code(
            validate_domain_bundle(bad_cand),
            ValidationRuleCode::unapproved_observation_in_knowledge,
            "unresolved candidate observation excluded from knowledge");
    }

    // 3. Conflicting observations: visible preservation, no silent overwrite, resolution rules.
    {
        auto bundle = make_synthetic_resistor_bundle();
        const ReviewerIdentity engineer{
            "eng.synthetic.reviewer-1",
            ActorKind::human_engineer,
            "Synthetic Lead Component Engineer"};
        const EvidenceId ev_id = bundle.evidence_locations.front().id;

        // Add two competing approved observations for max_overload_voltage (150 V vs 200 V).
        auto make_overload_obs = [&](const std::string& id,
                                     const std::string& val_text,
                                     double val_num,
                                     const std::string& dec_prefix) {
            ParameterObservation obs;
            obs.id = ObservationId{id};
            obs.component_id = bundle.component.id;
            obs.parameter_name = "V_OVERLOAD_MAX";
            obs.original_value_text = val_text;
            obs.original_unit_symbol = "V";
            obs.parsed_numeric_value = val_num;
            obs.qualifier = ValueQualifier::maximum;
            obs.category = ParameterCategory::absolute_maximum_rating;
            obs.operating_conditions = {
                OperatingCondition{"t_pulse", "<= 5", "s", std::nullopt, std::nullopt}};
            obs.evidence_refs = {ev_id};
            obs.extraction_method = ExtractionMethod::synthetic_fixture;
            apply_review_decision(
                obs,
                ReviewDecision{
                    ReviewDecisionId{dec_prefix + ".in"},
                    engineer,
                    "2026-10-31T13:00:00Z",
                    "Queued.",
                    {ev_id},
                    ReviewState::candidate,
                    ReviewState::needs_review,
                    std::nullopt});
            apply_review_decision(
                obs,
                ReviewDecision{
                    ReviewDecisionId{dec_prefix + ".ok"},
                    engineer,
                    "2026-10-31T13:05:00Z",
                    "Confirmed conflicting table/footnote entry.",
                    {ev_id},
                    ReviewState::needs_review,
                    ReviewState::approved,
                    std::nullopt});
            return obs;
        };

        const auto obs_150 = make_overload_obs("obs.res.overload.150", "150", 150.0, "rev.ov150");
        const auto obs_200 = make_overload_obs("obs.res.overload.200", "200", 200.0, "rev.ov200");
        bundle.observations.push_back(obs_150);
        bundle.observations.push_back(obs_200);

        // Omitting the conflict from ComponentKnowledge::conflicts fails bundle validation!
        require_invalid_with_code(
            validate_domain_bundle(bundle),
            ValidationRuleCode::undeclared_parameter_conflict,
            "undocumented competing approved observations detected");

        // Record visible unresolved conflict without promoting either value -> bundle valid!
        ParameterConflict conflict;
        conflict.conflict_id = "conflict.res.overload_voltage";
        conflict.parameter_name = "max_overload_voltage";
        conflict.competing_observations = {obs_150.id, obs_200.id};
        conflict.status = ConflictResolutionStatus::unresolved;
        conflict.description = "Summary header states 200 V while Table 1 footnote states 150 V.";
        bundle.knowledge->conflicts.push_back(conflict);
        require(bundle.knowledge->has_unresolved_conflicts(), "unresolved conflict flagged");
        require(validate_domain_bundle(bundle).valid(),
                "visible unresolved conflict with neither value promoted is valid");

        // Attempting to promote an observation from an unresolved conflict is blocked!
        auto np_150 = make_normalized_parameter(
            NormalizedParameterId{"np.res.overload.150"},
            obs_150,
            "max_overload_voltage",
            "Maximum short-term overload voltage stress limit",
            150.0,
            "V",
            false,
            "identity: 150 V = 150.0 V",
            obs_150.operating_conditions);
        rejects([&] {
            promote_parameter_to_knowledge(*bundle.knowledge, obs_150, np_150);
        }, "unresolved conflict blocks promotion");

        // Resolve conflict explicitly in favor of conservative 150 V reading by human engineer.
        bundle.knowledge->conflicts.front().status = ConflictResolutionStatus::resolved_by_review;
        bundle.knowledge->conflicts.front().selected_observation_id = obs_150.id;
        bundle.knowledge->conflicts.front().reviewer = engineer;
        bundle.knowledge->conflicts.front().resolution_rationale =
            "Adopted conservative Table 1 footnote limit (150 V) while retaining 200 V claim in conflict log.";
        require(!bundle.knowledge->has_unresolved_conflicts(), "conflict now resolved");

        // Promoting the selected 150 V observation succeeds; promoting losing 200 V fails (no overwrite!).
        promote_parameter_to_knowledge(*bundle.knowledge, obs_150, np_150);
        require(validate_domain_bundle(bundle).valid(), "resolved conflict promotion valid");

        auto np_200 = make_normalized_parameter(
            NormalizedParameterId{"np.res.overload.200"},
            obs_200,
            "max_overload_voltage",
            "Competing overload voltage stress limit",
            200.0,
            "V",
            false,
            "identity: 200 V = 200.0 V",
            obs_200.operating_conditions);
        rejects([&] {
            promote_parameter_to_knowledge(*bundle.knowledge, obs_200, np_200);
        }, "losing or duplicate slot cannot overwrite promoted parameter");
    }

    // 4. Reversed min/typ/max bounds and altered original notation/category rejected.
    {
        auto bundle = make_synthetic_resistor_bundle();
        // Mutate normalized minimum resistance from 999.0 to 1005.0 (> typical 1000.0).
        bundle.knowledge->normalized_parameters[0].normalized_numeric_value = 1005.0;
        require_invalid_with_code(
            validate_domain_bundle(bundle),
            ValidationRuleCode::reversed_qualifier_bounds,
            "reversed min/typ bounds rejected");

        auto bad_notation = make_synthetic_resistor_bundle();
        bad_notation.knowledge->normalized_parameters[0].preserved_original_value_text = "999";
        require_invalid_with_code(
            validate_domain_bundle(bad_notation),
            ValidationRuleCode::original_notation_mismatch,
            "altered preserved_original_value_text rejected");

        auto bad_category = make_synthetic_resistor_bundle();
        for (auto& np : bad_category.knowledge->normalized_parameters) {
            if (np.category == ParameterCategory::absolute_maximum_rating) {
                np.category = ParameterCategory::recommended_operating_condition;
            }
        }
        require_invalid_with_code(
            validate_domain_bundle(bad_category),
            ValidationRuleCode::category_mismatch,
            "reclassifying absolute_maximum_rating as recommended_operating_condition rejected");
    }

    // 5. Version and provenance consistency.
    {
        auto bundle = make_synthetic_resistor_bundle();
        const ReviewerIdentity engineer{
            "eng.synthetic.reviewer-1",
            ActorKind::human_engineer,
            std::nullopt};
        const auto kn_v2 = create_next_knowledge_revision(
            *bundle.knowledge,
            ComponentKnowledgeId{"ck.synthetic.syn-tf0603-1k00-lf.v2"},
            ReviewDecision{
                ReviewDecisionId{"rev.res.knowledge.v2"},
                engineer,
                "2026-10-31T14:00:00Z",
                "Re-reviewed synthetic resistor knowledge v2.",
                {bundle.evidence_locations.front().id},
                ReviewState::needs_review,
                ReviewState::approved,
                std::nullopt},
            "Revision 2 audit confirmation.");
        require(kn_v2.version == 2U, "version incremented to 2");
        require(kn_v2.previous_version_id == bundle.knowledge->id, "previous_version_id linked");
        require(kn_v2.version_history.size() == 2U, "two version_history entries retained");
        bundle.knowledge = kn_v2;
        bundle.models.front().component_knowledge_id = kn_v2.id;
        require(validate_domain_bundle(bundle).valid(), "version 2 bundle valid");

        // Break version history linkage.
        auto broken_ver = kn_v2;
        broken_ver.previous_version_id = std::nullopt;
        require_invalid_with_code(
            validate_component_knowledge(broken_ver),
            ValidationRuleCode::invalid_version_history,
            "version > 1 missing previous_version_id rejected");

        // Synthetic provenance mismatch: non-synthetic component using synthetic document.
        auto spoof_real = make_synthetic_resistor_bundle();
        spoof_real.component.is_synthetic = false;
        spoof_real.knowledge->component.is_synthetic = false;
        require_invalid_with_code(
            validate_domain_bundle(spoof_real),
            ValidationRuleCode::synthetic_provenance_mismatch,
            "synthetic document feeding non-synthetic component rejected");
    }
}

void test_model_artifact_and_validation_rules() {
    const auto res_bundle = make_synthetic_resistor_bundle();
    const auto valid_model = res_bundle.models.front();
    require(validate_model_artifact_descriptor(valid_model).valid(), "valid resistor model");

    // 1. Invalid pin mappings.
    {
        auto one_pin = valid_model;
        one_pin.terminals = {TerminalDescriptor{1U, "p1", "1", std::nullopt}};
        require_invalid_with_code(
            validate_model_artifact_descriptor(one_pin),
            ValidationRuleCode::invalid_terminal_mapping,
            "single-terminal electrical model rejected");

        auto reversed_pos = valid_model;
        reversed_pos.terminals[0].position = 2U;
        reversed_pos.terminals[1].position = 1U;
        require_invalid_with_code(
            validate_model_artifact_descriptor(reversed_pos),
            ValidationRuleCode::invalid_terminal_mapping,
            "out-of-order terminal positions rejected");

        auto dup_name = valid_model;
        dup_name.terminals[1].terminal_name = "p1";
        require_invalid_with_code(
            validate_model_artifact_descriptor(dup_name),
            ValidationRuleCode::invalid_terminal_mapping,
            "duplicate terminal_name rejected");

        auto dup_pin = valid_model;
        dup_pin.terminals[1].physical_pin_id = "1";
        require_invalid_with_code(
            validate_model_artifact_descriptor(dup_pin),
            ValidationRuleCode::invalid_terminal_mapping,
            "duplicate physical_pin_id rejected");
    }

    // 2. Invalid model version.
    for (const char* bad_ver : {"", "   ", "1.0 beta", "1.0\n"}) {
        auto bad = valid_model;
        bad.version = bad_ver;
        require_invalid_with_code(
            validate_model_artifact_descriptor(bad),
            ValidationRuleCode::invalid_model_version,
            "invalid model version rejected");
    }

    // 3. Missing source/license/artifact metadata where required.
    {
        auto no_vendor = valid_model;
        no_vendor.source_vendor = "";
        require_invalid_with_code(
            validate_model_artifact_descriptor(no_vendor),
            ValidationRuleCode::missing_model_source_or_license,
            "missing source_vendor rejected");

        auto no_license = valid_model;
        no_license.license.license_identifier = "";
        require_invalid_with_code(
            validate_model_artifact_descriptor(no_license),
            ValidationRuleCode::missing_model_source_or_license,
            "missing license_identifier rejected");

        auto no_restrictions = valid_model;
        no_restrictions.license.usage_restrictions = "  ";
        require_invalid_with_code(
            validate_model_artifact_descriptor(no_restrictions),
            ValidationRuleCode::missing_model_source_or_license,
            "missing usage_restrictions rejected");

        auto subckt_no_uri = valid_model;
        subckt_no_uri.format = ModelFormat::spice_subcircuit;
        subckt_no_uri.artifact_uri = std::nullopt;
        require_invalid_with_code(
            validate_model_artifact_descriptor(subckt_no_uri),
            ValidationRuleCode::missing_artifact_reference,
            "external subcircuit without artifact_uri rejected");
    }

    // 4. Invalid validity envelope.
    {
        auto empty_env = valid_model;
        empty_env.validity_envelope.operating_ranges.clear();
        require_invalid_with_code(
            validate_model_artifact_descriptor(empty_env),
            ValidationRuleCode::invalid_validity_envelope,
            "empty validity envelope rejected");

        auto inverted_range = valid_model;
        inverted_range.validity_envelope.operating_ranges[0].min_value = 125.0;
        inverted_range.validity_envelope.operating_ranges[0].max_value = -55.0;
        require_invalid_with_code(
            validate_model_artifact_descriptor(inverted_range),
            ValidationRuleCode::invalid_validity_envelope,
            "inverted validity range rejected");

        auto open_point = valid_model;
        open_point.validity_envelope.operating_ranges[0].min_value = 25.0;
        open_point.validity_envelope.operating_ranges[0].max_value = 25.0;
        open_point.validity_envelope.operating_ranges[0].min_inclusive = false;
        require_invalid_with_code(
            validate_model_artifact_descriptor(open_point),
            ValidationRuleCode::invalid_validity_envelope,
            "empty half-open single-point range rejected");

        auto unbounded_range = valid_model;
        unbounded_range.validity_envelope.operating_ranges[0].min_value = std::nullopt;
        unbounded_range.validity_envelope.operating_ranges[0].max_value = std::nullopt;
        require_invalid_with_code(
            validate_model_artifact_descriptor(unbounded_range),
            ValidationRuleCode::invalid_validity_envelope,
            "range with neither min nor max rejected");

        auto nan_range = valid_model;
        nan_range.validity_envelope.operating_ranges[0].max_value =
            std::numeric_limits<double>::infinity();
        require_invalid_with_code(
            validate_model_artifact_descriptor(nan_range),
            ValidationRuleCode::invalid_validity_envelope,
            "non-finite validity bound rejected");
    }

    // 5. Absolute maximum rating misused as behavioral model parameter rejected.
    {
        auto bad_bundle = make_synthetic_resistor_bundle();
        bad_bundle.models.front().parameter_mappings.push_back(ModelParameterMapping{
            "P_OPERATING",
            NormalizedParameterId{"np.res.p_diss.abs_max"},
            std::nullopt,
            "W",
            "Illegal binding of stress limit as nominal parameter"});
        require_invalid_with_code(
            validate_domain_bundle(bad_bundle),
            ValidationRuleCode::absolute_maximum_misused_as_model_parameter,
            "absolute_maximum_rating cannot bind to behavioral model parameter");
    }

    // 6. Four-way independence of observation approval, knowledge approval, model registration,
    //    and model validation status.
    {
        auto cap_bundle = make_synthetic_capacitor_bundle();
        // Capacitor bundle has approved observations, approved knowledge, and a valid registered
        // model descriptor, yet zero ModelValidationRecords -> must be unvalidated!
        require(validate_domain_bundle(cap_bundle).valid(), "capacitor bundle valid");
        require(
            effective_model_validation_status(
                cap_bundle.models.front(), cap_bundle.model_validations) ==
                ModelValidationStatus::unvalidated,
            "approved knowledge + valid descriptor remain unvalidated without validation record");

        // Resistor bundle has an explicit unvalidated record -> also unvalidated.
        auto bundle = make_synthetic_resistor_bundle();
        require(
            effective_model_validation_status(bundle.models.front(), bundle.model_validations) ==
                ModelValidationStatus::unvalidated,
            "explicit unvalidated record reports unvalidated");

        // LLM cannot author a validated_within_envelope record.
        ModelValidationRecord llm_val;
        llm_val.id = ModelValidationId{"mval.llm"};
        llm_val.model_id = bundle.models.front().id;
        llm_val.model_version = bundle.models.front().version;
        llm_val.status = ModelValidationStatus::validated_within_envelope;
        llm_val.validation_kind = ModelValidationKind::analytical_comparison;
        llm_val.reviewer = ReviewerIdentity{"llm.validator", ActorKind::llm_assistant, std::nullopt};
        llm_val.timestamp = "2026-10-31T15:00:00Z";
        llm_val.summary = "Attempted LLM model validation.";
        llm_val.evidence_refs = {bundle.evidence_locations.front().id};
        llm_val.verified_analyses = {AnalysisType::dc_operating_point};
        require_invalid_with_code(
            validate_model_validation_record(llm_val),
            ValidationRuleCode::unauthorized_reviewer_kind,
            "LLM model validation rejected");

        // Human engineer validates version 1.0.0 within envelope -> status becomes validated.
        ModelValidationRecord human_val = llm_val;
        human_val.id = ModelValidationId{"mval.res.v100.ok"};
        human_val.reviewer = ReviewerIdentity{
            "eng.model.verifier",
            ActorKind::human_engineer,
            "Synthetic Model Verification Engineer"};
        human_val.summary = "Verified linear ohmic equation metadata consistency at DC (synthetic test).";
        bundle.model_validations = {human_val};
        bundle.models.front().validation_record_refs = {human_val.id};
        require(validate_domain_bundle(bundle).valid(), "human model validation record valid");
        require(
            effective_model_validation_status(bundle.models.front(), bundle.model_validations) ==
                ModelValidationStatus::validated_within_envelope,
            "validated_within_envelope reported for matching version");

        // Updating model version to 1.1.0 without re-validating turns effective status out_of_date!
        bundle.models.front().version = "1.1.0";
        require(validate_domain_bundle(bundle).valid(), "updated model descriptor still valid");
        require(
            effective_model_validation_status(bundle.models.front(), bundle.model_validations) ==
                ModelValidationStatus::out_of_date,
            "version bump marks prior validation out_of_date");

        // Failed validation record reports failed_validation while observations and knowledge stay approved.
        ModelValidationRecord failed_val;
        failed_val.id = ModelValidationId{"mval.res.v110.fail"};
        failed_val.model_id = bundle.models.front().id;
        failed_val.model_version = "1.1.0";
        failed_val.status = ModelValidationStatus::failed_validation;
        failed_val.validation_kind = ModelValidationKind::analytical_comparison;
        failed_val.reviewer = human_val.reviewer;
        failed_val.timestamp = "2026-10-31T15:30:00Z";
        failed_val.summary = "Synthetic out-of-tolerance parameter check failure.";
        bundle.model_validations.push_back(failed_val);
        bundle.models.front().validation_record_refs.push_back(failed_val.id);
        require(validate_domain_bundle(bundle).valid(),
                "bundle with recorded failed model validation remains structurally valid");
        require(
            effective_model_validation_status(bundle.models.front(), bundle.model_validations) ==
                ModelValidationStatus::failed_validation,
            "failed model validation reported independently of approved component knowledge");
        require(bundle.knowledge->review_decision.new_state == ReviewState::approved,
                "component knowledge approval unaffected by model validation failure");
    }
}

} // namespace

int main() {
    try {
        test_synthetic_fixtures_baseline();
        test_component_identity_rules();
        test_evidence_and_source_document_rules();
        test_parameter_observation_rules();
        test_review_lifecycle_rules();
        test_component_knowledge_rules();
        test_model_artifact_and_validation_rules();

        std::cout << "PASS: " << checks << " component domain checks\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
