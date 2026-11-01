#include "datasim/component/model/model_validation.hpp"
#include <algorithm>

namespace datasim::component {

ModelValidationStatus effective_model_validation_status(
    const ModelArtifactDescriptor& model,
    const std::vector<ModelValidationRecord>& records) {
    if (model.validation_record_refs.empty()) {
        return ModelValidationStatus::unvalidated;
    }

    bool has_current_validated = false;
    bool has_current_out_of_date = false;
    bool has_stale_version_record = false;

    for (const auto& ref_id : model.validation_record_refs) {
        const auto it = std::find_if(records.begin(), records.end(), [&](const auto& rec) {
            return rec.id == ref_id && rec.model_id == model.id;
        });
        if (it == records.end()) {
            continue;
        }
        if (it->model_version != model.version) {
            has_stale_version_record = true;
            continue;
        }
        switch (it->status) {
            case ModelValidationStatus::failed_validation:
                return ModelValidationStatus::failed_validation;
            case ModelValidationStatus::out_of_date:
                has_current_out_of_date = true;
                break;
            case ModelValidationStatus::validated_within_envelope:
                has_current_validated = true;
                break;
            case ModelValidationStatus::unvalidated:
                break;
        }
    }

    if (has_current_out_of_date) {
        return ModelValidationStatus::out_of_date;
    }
    if (has_current_validated) {
        return ModelValidationStatus::validated_within_envelope;
    }
    if (has_stale_version_record) {
        return ModelValidationStatus::out_of_date;
    }
    return ModelValidationStatus::unvalidated;
}

} // namespace datasim::component
