#include "intake_fixtures.hpp"
#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/observation/review.hpp"
#include "datasim/component/validation/domain_validation.hpp"
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

template <typename Function>
void require_invalid_argument(Function&& function, const char* expression) {
    ++checks;
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error(std::string("expected std::invalid_argument: ") + expression);
}

} // namespace

int main() {
    try {
        using namespace datasim::component;
        using namespace datasim::intake;

        const auto manifest = datasim::intake::testing::make_synthetic_resistor_manifest();
        auto candidate_result = build_candidate_bundle(manifest);
        require(candidate_result.bundle.has_value(), "candidate handoff bundle exists");
        auto bundle = *candidate_result.bundle;
        auto& observation = bundle.observations.front();

        require_invalid_argument(
            [&] {
                make_normalized_parameter(
                    NormalizedParameterId{"np.before-approval"},
                    observation,
                    "resistance",
                    "Resistance",
                    1000.0,
                    "ohm",
                    false,
                    "explicit_scale",
                    observation.operating_conditions);
            },
            "candidate cannot be normalized");

        ComponentKnowledge knowledge;
        knowledge.id = ComponentKnowledgeId{"ck.intake.handoff.v1"};
        knowledge.component = bundle.component;
        require_invalid_argument(
            [&] {
                promote_parameter_to_knowledge(
                    knowledge,
                    observation,
                    NormalizedParameter{});
            },
            "candidate cannot be promoted");

        const ReviewerIdentity human{
            "eng.intake.handoff",
            ActorKind::human_engineer,
            "Synthetic review engineer"};
        ReviewDecision direct_approval{
            ReviewDecisionId{"rev.intake.direct"},
            human,
            "2026-11-04T11:00:00Z",
            "Direct approval is intentionally invalid.",
            observation.evidence_refs,
            ReviewState::candidate,
            ReviewState::approved,
            std::nullopt};
        require_invalid_argument(
            [&] { apply_review_decision(observation, direct_approval); },
            "direct candidate approval rejected");

        ReviewDecision queue{
            ReviewDecisionId{"rev.intake.queue"},
            ReviewerIdentity{
                "check.intake.structure",
                ActorKind::automated_check,
                "Synthetic structural checker"},
            "2026-11-04T11:01:00Z",
            "Structural intake checks passed.",
            observation.evidence_refs,
            ReviewState::candidate,
            ReviewState::needs_review,
            std::nullopt};
        apply_review_decision(observation, queue);
        require(observation.review_state == ReviewState::needs_review,
                "candidate transitions to needs_review");

        ReviewDecision approval{
            ReviewDecisionId{"rev.intake.approve"},
            human,
            "2026-11-04T11:02:00Z",
            "Reviewed synthetic evidence, units, qualifier, and conditions.",
            observation.evidence_refs,
            ReviewState::needs_review,
            ReviewState::approved,
            std::nullopt};
        apply_review_decision(observation, approval);
        require(observation.review_state == ReviewState::approved,
                "needs_review transitions to approved");
        require(observation.review_history.size() == 2U,
                "review history retained");

        auto invalid_kind = observation;
        invalid_kind.review_state = ReviewState::needs_review;
        invalid_kind.review_history.pop_back();
        ReviewDecision parser_approval = approval;
        parser_approval.reviewer = {
            "parser",
            ActorKind::automatic_parser,
            std::nullopt};
        require_invalid_argument(
            [&] { apply_review_decision(invalid_kind, parser_approval); },
            "automatic parser cannot approve");

        auto invalid_rationale = observation;
        invalid_rationale.review_state = ReviewState::needs_review;
        invalid_rationale.review_history.pop_back();
        auto empty_rationale = approval;
        empty_rationale.rationale.clear();
        require_invalid_argument(
            [&] { apply_review_decision(invalid_rationale, empty_rationale); },
            "empty rationale rejected");

        auto invalid_reference = observation;
        invalid_reference.review_state = ReviewState::needs_review;
        invalid_reference.review_history.pop_back();
        auto bad_reference = approval;
        bad_reference.evidence_refs = {EvidenceId{"ev.unknown"}};
        apply_review_decision(invalid_reference, bad_reference);
        auto invalid_reference_bundle = bundle;
        invalid_reference_bundle.observations.front() = invalid_reference;
        require(!validate_domain_bundle(invalid_reference_bundle).valid(),
                "unknown evidence reference rejected by aggregate validation");

        auto normalized = make_normalized_parameter(
            NormalizedParameterId{"np.intake.handoff.resistance"},
            observation,
            "resistance",
            "Typical resistance",
            1000.0,
            "ohm",
            false,
            "explicit_scale: 1.000 kohm * 1000 = 1000 ohm",
            observation.operating_conditions);
        require(normalized.preserved_original_value_text == "1.000",
                "normalized original text preserved");
        require(normalized.preserved_original_unit_symbol == "kohm",
                "normalized original unit preserved");

        knowledge.review_decision = {
            ReviewDecisionId{"rev.intake.knowledge"},
            human,
            "2026-11-04T11:03:00Z",
            "Approved synthetic knowledge snapshot.",
            observation.evidence_refs,
            ReviewState::needs_review,
            ReviewState::approved,
            std::nullopt};
        knowledge.version_history.push_back({
            1U,
            knowledge.id,
            std::nullopt,
            human,
            "2026-11-04T11:03:00Z",
            "Initial synthetic knowledge snapshot."});
        promote_parameter_to_knowledge(knowledge, observation, normalized);
        bundle.knowledge = knowledge;
        require(validate_domain_bundle(bundle).valid(),
                "reviewed handoff domain bundle is valid");
        require(bundle.component.is_synthetic &&
                    bundle.documents.front().is_synthetic,
                "synthetic provenance remains synthetic");

        std::cout << "intake handoff checks: " << checks << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "intake handoff test failure after " << checks << " checks: "
                  << error.what() << '\n';
        return 1;
    }
}
