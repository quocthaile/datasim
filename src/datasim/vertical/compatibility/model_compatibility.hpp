#pragma once
#include "datasim/component/model/model_artifact.hpp"
#include "datasim/component/model/model_validation.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include "datasim/vertical/resolution/parameter_resolution.hpp"
#include "datasim/vertical/validation/vertical_validation.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::vertical {

enum class ExecutionPolicy {
    engineering_approved_only,
    test_only_synthetic
};

enum class ExecutionClassification {
    engineering_approved,
    test_only_synthetic
};

struct OperatingEnvironment {
    double ambient_temperature_degC = 27.0;
    double stimulus_voltage_V = 5.0;
    double initial_voltage_V = 0.0;
    std::vector<component::OperatingCondition> additional_conditions;

    auto operator<=>(const OperatingEnvironment&) const = default;
};

struct ModelCompatibilityReport {
    VerticalValidationReport validation;
    ExecutionClassification classification = ExecutionClassification::engineering_approved;
    component::ModelValidationStatus resistor_validation_status =
        component::ModelValidationStatus::unvalidated;
    component::ModelValidationStatus capacitor_validation_status =
        component::ModelValidationStatus::unvalidated;
    std::vector<component::ModelValidationId> resistor_validation_record_ids;
    std::vector<component::ModelValidationId> capacitor_validation_record_ids;

    [[nodiscard]] bool compatible() const noexcept { return validation.valid(); }

    auto operator<=>(const ModelCompatibilityReport&) const = default;
};

[[nodiscard]] constexpr bool is_model_format_supported_by_rc_backend(
    component::ModelFormat format) noexcept {
    return format == component::ModelFormat::analytical_equation;
}

[[nodiscard]] constexpr bool is_analysis_supported_by_rc_backend(
    component::AnalysisType analysis) noexcept {
    return analysis == component::AnalysisType::transient;
}

[[nodiscard]] ModelCompatibilityReport evaluate_rc_model_compatibility(
    const CircuitAssembly& assembly,
    const ResolvedRcParameters& resolved_parameters,
    component::AnalysisType requested_analysis,
    const OperatingEnvironment& environment,
    ExecutionPolicy policy = ExecutionPolicy::engineering_approved_only);

} // namespace datasim::vertical

