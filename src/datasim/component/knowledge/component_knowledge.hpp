#pragma once
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/observation/review.hpp"
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

enum class ConflictResolutionStatus {
    unresolved,
    resolved_by_review
};

// Keeps competing observations permanently visible; never silently averages or overwrites.
struct ParameterConflict {
    std::string conflict_id;
    std::string parameter_name;
    std::vector<ObservationId> competing_observations;
    ConflictResolutionStatus status = ConflictResolutionStatus::unresolved;
    std::optional<ObservationId> selected_observation_id;
    std::string description;
    std::optional<ReviewerIdentity> reviewer;
    std::optional<std::string> resolution_rationale;

    auto operator<=>(const ParameterConflict&) const = default;
};

struct KnowledgeRevisionEntry {
    std::uint32_t version = 1;
    ComponentKnowledgeId knowledge_id;
    std::optional<ComponentKnowledgeId> previous_knowledge_id;
    ReviewerIdentity reviewer;
    std::string timestamp;
    std::string summary;

    auto operator<=>(const KnowledgeRevisionEntry&) const = default;
};

// Versioned, reviewed component facts traceable to approved observations.
// Approval of ComponentKnowledge does not validate any mathematical or simulation model.
struct ComponentKnowledge {
    ComponentKnowledgeId id;
    ComponentIdentity component;
    std::uint32_t version = 1;
    std::optional<ComponentKnowledgeId> previous_version_id;
    std::vector<ObservationId> approved_observation_refs;
    std::vector<NormalizedParameter> normalized_parameters;
    std::vector<ParameterConflict> conflicts;
    ReviewDecision review_decision;
    std::vector<KnowledgeRevisionEntry> version_history;

    [[nodiscard]] bool has_unresolved_conflicts() const noexcept {
        for (const auto& c : conflicts) {
            if (c.status == ConflictResolutionStatus::unresolved) {
                return true;
            }
        }
        return false;
    }

    auto operator<=>(const ComponentKnowledge&) const = default;
};

// Promotes an approved observation and its normalized representation into ComponentKnowledge,
// rejecting unapproved observations, unresolved conflict promotion, or silent overwrites.
void promote_parameter_to_knowledge(
    ComponentKnowledge& knowledge,
    const ParameterObservation& observation,
    NormalizedParameter parameter);

// Creates the next sequential ComponentKnowledge revision preserving lineage and prior history.
ComponentKnowledge create_next_knowledge_revision(
    const ComponentKnowledge& previous,
    ComponentKnowledgeId new_id,
    ReviewDecision new_review_decision,
    std::string revision_summary);

} // namespace datasim::component
