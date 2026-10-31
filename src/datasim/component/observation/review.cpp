#include "datasim/component/observation/review.hpp"
#include "datasim/component/observation/parameter_observation.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include <stdexcept>

namespace datasim::component {

void apply_review_decision(ParameterObservation& observation, const ReviewDecision& decision) {
    if (decision.previous_state != observation.review_state) {
        throw std::invalid_argument(
            "review decision previous_state does not match current observation review_state");
    }

    ParameterObservation candidate_next = observation;
    candidate_next.review_history.push_back(decision);
    candidate_next.review_state = decision.new_state;
    if (decision.new_state == ReviewState::superseded) {
        candidate_next.superseded_by = decision.superseding_observation_id;
    } else {
        candidate_next.superseded_by = std::nullopt;
    }

    const ValidationReport decision_report = validate_review_decision(decision);
    ensure_valid(decision_report);

    const ValidationReport obs_report = validate_parameter_observation(candidate_next);
    ensure_valid(obs_report);

    observation = std::move(candidate_next);
}

} // namespace datasim::component
