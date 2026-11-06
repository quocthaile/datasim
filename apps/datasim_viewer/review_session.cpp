#include "review_session.hpp"

#include "datasim/component/validation/domain_validation.hpp"
#include "datasim/component/observation/review.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>

namespace datasim_viewer {
using namespace datasim::component;

ReviewSession::ReviewSession(std::vector<ParameterObservation> observations,
                             std::vector<EvidenceLocation> evidence_locations,
                             std::string source_fingerprint,
                             std::string extraction_configuration)
    : evidence_locations_(std::move(evidence_locations)),
      source_fingerprint_(std::move(source_fingerprint)),
      extraction_configuration_(std::move(extraction_configuration)) {
    if (source_fingerprint_.empty() || extraction_configuration_.empty())
        throw std::invalid_argument("Review session requires source and extraction configuration fingerprints");
    std::set<EvidenceId> known_evidence;
    for (const auto& location : evidence_locations_) {
        if (location.id.empty() || !known_evidence.insert(location.id).second)
            throw std::invalid_argument("Review session evidence metadata contains an empty or duplicate EvidenceId");
    }
    for (auto& item : observations) {
        if (item.id.empty()) throw std::invalid_argument("Review session observation has an empty ObservationId");
        const auto id = item.id;
        if (!observations_.emplace(id, std::move(item)).second)
            throw std::invalid_argument("Review session contains duplicate ObservationId");
        ids_.push_back(id);
    }
    for (const auto& [id, item] : observations_) {
        if (item.evidence_refs.empty()) throw std::invalid_argument("Observation " + id.value + " has no source evidence");
        for (const auto& evidence : item.evidence_refs)
            if (!known_evidence.contains(evidence))
                throw std::invalid_argument("Observation " + id.value + " references evidence absent from this extraction");
    }
}

const ParameterObservation& ReviewSession::observation(const ObservationId& id) const {
    const auto found = observations_.find(id);
    if (found == observations_.end()) throw std::out_of_range("ObservationId is not owned by this review session");
    return found->second;
}

const std::vector<ObservationId>& ReviewSession::observation_ids() const noexcept { return ids_; }
const std::vector<EvidenceLocation>& ReviewSession::evidence_locations() const noexcept { return evidence_locations_; }
const std::string& ReviewSession::source_fingerprint() const noexcept { return source_fingerprint_; }
const std::string& ReviewSession::extraction_configuration() const noexcept { return extraction_configuration_; }
bool ReviewSession::edited() const noexcept { return edited_; }
bool ReviewSession::valid() const noexcept { return valid_; }

std::vector<EvidenceId> ReviewSession::evidence_for(const ObservationId& id) const {
    return observation(id).evidence_refs;
}

void ReviewSession::apply(const ObservationId& id, const ReviewDecision& decision,
                          const std::string& decision_context) {
    if (!valid_) throw std::logic_error("Review session was invalidated by source/configuration change");
    if (decision_context != source_fingerprint_ + "\n" + extraction_configuration_)
        throw std::logic_error("Review decision belongs to a stale source or extraction configuration");
    auto target_it = observations_.find(id);
    if (target_it == observations_.end()) throw std::out_of_range("ObservationId is not owned by this review session");
    auto& target = target_it->second;
    if (decision.evidence_refs.empty() || std::any_of(decision.evidence_refs.begin(), decision.evidence_refs.end(),
        [&](const auto& ref) { return std::find(target.evidence_refs.begin(), target.evidence_refs.end(), ref) == target.evidence_refs.end(); }))
        throw std::invalid_argument("Review decision evidence must belong to the selected observation");
    for (const auto& [observation_id, owned] : observations_) {
        (void)observation_id;
        if (std::any_of(owned.review_history.begin(), owned.review_history.end(),
            [&](const auto& prior) { return prior.id == decision.id; }))
            throw std::invalid_argument("ReviewDecisionId is already present in this session");
    }
    if (decision.new_state == ReviewState::superseded) {
        if (!decision.superseding_observation_id) throw std::invalid_argument("Supersession requires a replacement ObservationId");
        const auto& replacement = observation(*decision.superseding_observation_id);
        if (*decision.superseding_observation_id == id) throw std::invalid_argument("An observation cannot supersede itself");
        if (replacement.component_id != target.component_id)
            throw std::invalid_argument("Replacement observation must belong to the same component");
        bool same_source = false;
        for (const auto& a : target.evidence_refs) for (const auto& b : replacement.evidence_refs)
            for (const auto& location : evidence_locations_)
                if (location.id == a && location.id == b) same_source = true;
        if (!same_source) throw std::invalid_argument("Replacement observation must share source evidence context");
    }
    auto updated = target;
    const auto report = validate_review_decision(decision);
    if (!report.valid()) {
        std::ostringstream message;
        for (const auto& issue : report.issues) { if (message.tellp() > 0) message << "; "; message << issue.entity_path << ": " << issue.message; }
        throw std::invalid_argument(message.str());
    }
    apply_review_decision(updated, decision);
    target = std::move(updated);
    edited_ = true;
}

void ReviewSession::invalidate() noexcept { valid_ = false; }
} // namespace datasim_viewer
