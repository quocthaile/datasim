#pragma once
#include "datasim/component/identity/ids.hpp"
#include "datasim/component/model/model_artifact.hpp"
#include "datasim/component/observation/review.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

enum class ModelValidationStatus {
    unvalidated,
    validated_within_envelope,
    failed_validation,
    out_of_date
};

enum class ModelValidationKind {
    none,
    analytical_comparison,
    reference_simulation_comparison,
    datasheet_curve_comparison,
    physical_measurement_comparison
};

// Independent assessment of a registered model artifact version against explicit evidence.
struct ModelValidationRecord {
    ModelValidationId id;
    ModelArtifactId model_id;
    std::string model_version;
    ModelValidationStatus status = ModelValidationStatus::unvalidated;
    ModelValidationKind validation_kind = ModelValidationKind::none;
    std::optional<ReviewerIdentity> reviewer;
    std::optional<std::string> timestamp;
    std::string summary;
    std::vector<EvidenceId> evidence_refs;
    std::vector<AnalysisType> verified_analyses;

    auto operator<=>(const ModelValidationRecord&) const = default;
};

// Computes effective validation status solely from linked ModelValidationRecords for the
// exact model ID and version; never infers validation from observation or knowledge approval.
[[nodiscard]] ModelValidationStatus effective_model_validation_status(
    const ModelArtifactDescriptor& model,
    const std::vector<ModelValidationRecord>& records);

} // namespace datasim::component
