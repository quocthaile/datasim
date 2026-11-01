#pragma once
#include <compare>
#include <string>

namespace datasim::component {

template <typename Tag>
struct StrongId {
    std::string value;

    auto operator<=>(const StrongId&) const = default;
    [[nodiscard]] bool empty() const noexcept { return value.empty(); }
};

struct ComponentIdTag {};
struct SourceDocumentIdTag {};
struct EvidenceIdTag {};
struct ObservationIdTag {};
struct ReviewDecisionIdTag {};
struct NormalizedParameterIdTag {};
struct ComponentKnowledgeIdTag {};
struct ModelArtifactIdTag {};
struct ModelValidationIdTag {};

using ComponentId = StrongId<ComponentIdTag>;
using SourceDocumentId = StrongId<SourceDocumentIdTag>;
using EvidenceId = StrongId<EvidenceIdTag>;
using ObservationId = StrongId<ObservationIdTag>;
using ReviewDecisionId = StrongId<ReviewDecisionIdTag>;
using NormalizedParameterId = StrongId<NormalizedParameterIdTag>;
using ComponentKnowledgeId = StrongId<ComponentKnowledgeIdTag>;
using ModelArtifactId = StrongId<ModelArtifactIdTag>;
using ModelValidationId = StrongId<ModelValidationIdTag>;

} // namespace datasim::component
