#include "datasim/component/knowledge/component_knowledge.hpp"
#include "datasim/component/knowledge/normalized_parameter.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace datasim::component {

namespace {

using ConditionKey = std::vector<std::tuple<std::string, std::string, std::string, std::string>>;

ConditionKey canonical_condition_key(const std::vector<OperatingCondition>& conditions) {
    ConditionKey key;
    key.reserve(conditions.size());
    for (const auto& c : conditions) {
        key.emplace_back(
            c.parameter_name,
            c.original_value_text,
            c.unit_symbol,
            c.notes.value_or(""));
    }
    std::sort(key.begin(), key.end());
    return key;
}

} // namespace

NormalizedParameter make_normalized_parameter(
    NormalizedParameterId id,
    const ParameterObservation& approved_observation,
    std::string canonical_name,
    std::string quantity_description,
    std::optional<double> normalized_numeric_value,
    std::optional<std::string> normalized_si_unit,
    bool unitless,
    std::string conversion_rule,
    std::vector<OperatingCondition> applicable_conditions) {
    if (approved_observation.review_state != ReviewState::approved) {
        throw std::invalid_argument(
            "only approved observations can be normalized into trusted parameters");
    }
    if (!approved_observation.original_value_text.has_value()) {
        throw std::invalid_argument(
            "observation with missing value text cannot be normalized");
    }

    NormalizedParameter param;
    param.id = std::move(id);
    param.canonical_name = std::move(canonical_name);
    param.quantity_description = std::move(quantity_description);
    param.qualifier = approved_observation.qualifier;
    param.category = approved_observation.category;
    param.normalized_numeric_value = normalized_numeric_value;
    param.normalized_si_unit = std::move(normalized_si_unit);
    param.unitless = unitless;
    param.source_observation_id = approved_observation.id;
    param.preserved_original_value_text = *approved_observation.original_value_text;
    param.preserved_original_unit_symbol = approved_observation.original_unit_symbol;
    param.conversion_rule = std::move(conversion_rule);
    param.applicable_conditions = std::move(applicable_conditions);

    ensure_valid(validate_normalized_parameter(param));
    return param;
}

void promote_parameter_to_knowledge(
    ComponentKnowledge& knowledge,
    const ParameterObservation& observation,
    NormalizedParameter parameter) {
    ensure_valid(validate_parameter_observation(observation));
    ensure_valid(validate_normalized_parameter(parameter));

    if (observation.component_id != knowledge.component.id) {
        throw std::invalid_argument("observation component_id does not match ComponentKnowledge");
    }
    if (observation.review_state != ReviewState::approved) {
        throw std::invalid_argument("only approved observations may enter ComponentKnowledge");
    }
    if (parameter.source_observation_id != observation.id) {
        throw std::invalid_argument(
            "NormalizedParameter source_observation_id does not match observation id");
    }
    if (parameter.qualifier != observation.qualifier ||
        parameter.category != observation.category ||
        !observation.original_value_text.has_value() ||
        parameter.preserved_original_value_text != *observation.original_value_text ||
        parameter.preserved_original_unit_symbol != observation.original_unit_symbol) {
        throw std::invalid_argument(
            "NormalizedParameter does not preserve observation notation, qualifier, or category");
    }

    for (const auto& conflict : knowledge.conflicts) {
        const bool participates = std::find(
                                      conflict.competing_observations.begin(),
                                      conflict.competing_observations.end(),
                                      observation.id) != conflict.competing_observations.end();
        if (!participates) {
            continue;
        }
        if (conflict.status == ConflictResolutionStatus::unresolved) {
            throw std::invalid_argument(
                "observation participating in an unresolved conflict cannot be promoted");
        }
        if (conflict.selected_observation_id != observation.id) {
            throw std::invalid_argument(
                "non-selected observation from a resolved conflict cannot be promoted");
        }
    }

    const ConditionKey incoming_cond = canonical_condition_key(parameter.applicable_conditions);
    for (const auto& existing : knowledge.normalized_parameters) {
        if (existing.id == parameter.id) {
            throw std::invalid_argument("duplicate NormalizedParameterId in ComponentKnowledge");
        }
        if (existing.canonical_name == parameter.canonical_name &&
            existing.category == parameter.category &&
            existing.qualifier == parameter.qualifier &&
            canonical_condition_key(existing.applicable_conditions) == incoming_cond) {
            throw std::invalid_argument(
                "conflicting or duplicate parameter slot cannot silently overwrite existing knowledge");
        }
    }

    if (std::find(
            knowledge.approved_observation_refs.begin(),
            knowledge.approved_observation_refs.end(),
            observation.id) == knowledge.approved_observation_refs.end()) {
        knowledge.approved_observation_refs.push_back(observation.id);
    }
    knowledge.normalized_parameters.push_back(std::move(parameter));
}

ComponentKnowledge create_next_knowledge_revision(
    const ComponentKnowledge& previous,
    ComponentKnowledgeId new_id,
    ReviewDecision new_review_decision,
    std::string revision_summary) {
    ensure_valid(validate_component_knowledge(previous));
    if (new_id == previous.id) {
        throw std::invalid_argument("next ComponentKnowledge revision must have a distinct id");
    }

    ComponentKnowledge next = previous;
    next.id = std::move(new_id);
    next.version = previous.version + 1U;
    next.previous_version_id = previous.id;
    next.review_decision = std::move(new_review_decision);

    KnowledgeRevisionEntry entry;
    entry.version = next.version;
    entry.knowledge_id = next.id;
    entry.previous_knowledge_id = previous.id;
    entry.reviewer = next.review_decision.reviewer;
    entry.timestamp = next.review_decision.timestamp;
    entry.summary = std::move(revision_summary);
    next.version_history.push_back(std::move(entry));

    ensure_valid(validate_component_knowledge(next));
    return next;
}

} // namespace datasim::component
