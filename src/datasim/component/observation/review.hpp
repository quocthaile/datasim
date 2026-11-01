#pragma once
#include "datasim/component/identity/ids.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

// Engineering review states for extracted data. Distinct from project charter statuses.
enum class ReviewState {
    candidate,
    needs_review,
    approved,
    rejected,
    superseded
};

enum class ActorKind {
    human_engineer,
    automated_check,
    automatic_parser,
    llm_assistant
};

struct ReviewerIdentity {
    std::string id;
    ActorKind kind = ActorKind::human_engineer;
    std::optional<std::string> display_name;

    auto operator<=>(const ReviewerIdentity&) const = default;
};

struct ReviewDecision {
    ReviewDecisionId id;
    ReviewerIdentity reviewer;
    std::string timestamp;
    std::string rationale;
    std::vector<EvidenceId> evidence_refs;
    ReviewState previous_state = ReviewState::candidate;
    ReviewState new_state = ReviewState::needs_review;
    std::optional<ObservationId> superseding_observation_id;

    auto operator<=>(const ReviewDecision&) const = default;
};

struct ParameterObservation;

[[nodiscard]] constexpr bool is_valid_review_transition(ReviewState from, ReviewState to) noexcept {
    switch (from) {
        case ReviewState::candidate:
            return to == ReviewState::needs_review;
        case ReviewState::needs_review:
            return to == ReviewState::approved || to == ReviewState::rejected;
        case ReviewState::approved:
            return to == ReviewState::superseded;
        case ReviewState::rejected:
        case ReviewState::superseded:
            return false;
    }
    return false;
}

[[nodiscard]] constexpr bool requires_human_engineer_reviewer(ReviewState target_state) noexcept {
    return target_state == ReviewState::approved ||
           target_state == ReviewState::rejected ||
           target_state == ReviewState::superseded;
}

// Deterministically validates and applies a review state transition with strong exception safety.
void apply_review_decision(ParameterObservation& observation, const ReviewDecision& decision);

} // namespace datasim::component
