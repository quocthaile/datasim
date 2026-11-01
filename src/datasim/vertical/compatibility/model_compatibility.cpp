#include "datasim/vertical/compatibility/model_compatibility.hpp"
#include <algorithm>
#include <cmath>
#include <string_view>

namespace datasim::vertical {

namespace {

bool value_within_constraint(
    double value,
    const component::NumericRangeConstraint& constraint) noexcept {
    if (!std::isfinite(value)) {
        return false;
    }
    if (constraint.min_value.has_value()) {
        if (constraint.min_inclusive) {
            if (value < *constraint.min_value) {
                return false;
            }
        } else {
            if (value <= *constraint.min_value) {
                return false;
            }
        }
    }
    if (constraint.max_value.has_value()) {
        if (constraint.max_inclusive) {
            if (value > *constraint.max_value) {
                return false;
            }
        } else {
            if (value >= *constraint.max_value) {
                return false;
            }
        }
    }
    return true;
}

bool quantity_matches(std::string_view cond_name, std::string_view constraint_name) noexcept {
    if (cond_name == constraint_name) {
        return true;
    }
    if (constraint_name == "ambient_temperature" &&
        (cond_name == "T_A" || cond_name == "T_ambient")) {
        return true;
    }
    if (constraint_name == "terminal_dc_voltage" &&
        (cond_name == "V_DC" || cond_name == "V_terminal" || cond_name == "voltage_V")) {
        return true;
    }
    if (constraint_name == "continuous_power" &&
        (cond_name == "P_load" || cond_name == "P_continuous" || cond_name == "power_W")) {
        return true;
    }
    if (constraint_name == "signal_frequency" &&
        (cond_name == "f_signal" || cond_name == "frequency_Hz")) {
        return true;
    }
    return false;
}

void evaluate_single_model(
    VerticalValidationReport& report,
    const CircuitAssembly& assembly,
    const ResolvedComponentParameter& resolved_param,
    component::AnalysisType requested_analysis,
    const OperatingEnvironment& environment,
    double circuit_resistance_ohm,
    component::ModelValidationStatus& out_status,
    std::vector<component::ModelValidationId>& out_record_ids) {
    const std::string mpath = "circuit_assembly[" + assembly.id.value +
                              "].instances[" + resolved_param.instance_id.value +
                              "].model[" + resolved_param.model_id.value + "]";

    const auto* bundle = find_bundle_for_component(assembly, resolved_param.component.id);
    if (bundle == nullptr) {
        return;
    }
    const auto* model = find_model_in_bundle(*bundle, resolved_param.model_id);
    if (model == nullptr) {
        return;
    }

    if (!is_model_format_supported_by_rc_backend(model->format)) {
        report.add(
            VerticalValidationCode::unsupported_model_format,
            mpath + ".format",
            "ideal-RC backend supports only ModelFormat::analytical_equation");
        if (model->format == component::ModelFormat::spice_subcircuit ||
            model->format == component::ModelFormat::spice_model_card ||
            model->format == component::ModelFormat::lookup_table ||
            model->format == component::ModelFormat::vendor_specific_artifact) {
            report.add(
                VerticalValidationCode::external_artifact_unavailable_in_rc_backend,
                mpath + ".artifact_uri",
                "external model artifact metadata does not imply runtime availability or loader support in the ideal-RC backend");
        }
    }

    const bool model_supports_analysis = std::find(
        model->supported_analyses.begin(),
        model->supported_analyses.end(),
        requested_analysis) != model->supported_analyses.end();
    if (!model_supports_analysis) {
        report.add(
            VerticalValidationCode::analysis_not_supported_by_model,
            mpath + ".supported_analyses",
            "selected ModelArtifactDescriptor does not declare support for the requested analysis");
    }

    // Inspect validation records for version mismatches and effective status.
    for (const auto& ref_id : model->validation_record_refs) {
        for (const auto& rec : bundle->model_validations) {
            if (rec.id == ref_id && rec.model_id == model->id) {
                if (rec.model_version != model->version) {
                    report.add(
                        VerticalValidationCode::model_version_mismatch,
                        mpath + ".validation_record[" + rec.id.value + "].model_version",
                        "ModelValidationRecord version '" + rec.model_version +
                            "' does not match selected ModelArtifactDescriptor version '" +
                            model->version + "'");
                }
            }
        }
    }

    out_status = component::effective_model_validation_status(*model, bundle->model_validations);
    if (out_status == component::ModelValidationStatus::unvalidated) {
        report.add(
            VerticalValidationCode::model_unvalidated,
            mpath + ".validation_status",
            "selected ModelArtifactDescriptor is unvalidated; approved knowledge does not validate a model");
    } else if (out_status == component::ModelValidationStatus::failed_validation) {
        report.add(
            VerticalValidationCode::model_failed_validation,
            mpath + ".validation_status",
            "selected ModelArtifactDescriptor has ModelValidationStatus::failed_validation");
    } else if (out_status == component::ModelValidationStatus::out_of_date) {
        report.add(
            VerticalValidationCode::model_validation_out_of_date,
            mpath + ".validation_status",
            "selected ModelArtifactDescriptor validation is out_of_date for version '" +
                model->version + "'");
    }

    bool analysis_verified_by_record = false;
    for (const auto& ref_id : model->validation_record_refs) {
        for (const auto& rec : bundle->model_validations) {
            if (rec.id == ref_id &&
                rec.model_id == model->id &&
                rec.model_version == model->version &&
                rec.status == component::ModelValidationStatus::validated_within_envelope) {
                out_record_ids.push_back(rec.id);
                if (std::find(
                        rec.verified_analyses.begin(),
                        rec.verified_analyses.end(),
                        requested_analysis) != rec.verified_analyses.end()) {
                    analysis_verified_by_record = true;
                }
            }
        }
    }

    if (out_status == component::ModelValidationStatus::validated_within_envelope &&
        !analysis_verified_by_record) {
        report.add(
            VerticalValidationCode::analysis_not_verified_in_validation_record,
            mpath + ".verified_analyses",
            "no current validated_within_envelope ModelValidationRecord verifies the requested analysis");
    }

    // Validate resolved parameter operating conditions against the model validity envelope.
    for (const auto& constraint : model->validity_envelope.operating_ranges) {
        const std::string cpath =
            mpath + ".validity_envelope[" + constraint.quantity_name + "]";

        if (constraint.quantity_name == "ambient_temperature") {
            if (constraint.unit_symbol != "degC") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "ambient_temperature validity envelope constraint must use 'degC'");
            } else if (!value_within_constraint(environment.ambient_temperature_degC, constraint)) {
                report.add(
                    VerticalValidationCode::operating_condition_outside_validity_envelope,
                    cpath,
                    "run ambient_temperature_degC falls outside model validity envelope");
            }
        } else if (constraint.quantity_name == "terminal_dc_voltage") {
            if (constraint.unit_symbol != "V") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "terminal_dc_voltage validity envelope constraint must use 'V'");
            } else if (!value_within_constraint(environment.stimulus_voltage_V, constraint) ||
                       !value_within_constraint(environment.initial_voltage_V, constraint)) {
                report.add(
                    VerticalValidationCode::operating_condition_outside_validity_envelope,
                    cpath,
                    "circuit terminal voltage falls outside model validity envelope");
            }
        } else if (constraint.quantity_name == "continuous_power" &&
                   resolved_param.role == CircuitElementRole::series_resistor) {
            if (constraint.unit_symbol != "W") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "continuous_power validity envelope constraint must use 'W'");
            } else if (std::isfinite(circuit_resistance_ohm) && circuit_resistance_ohm > 0.0) {
                const double peak_power_W =
                    (environment.stimulus_voltage_V * environment.stimulus_voltage_V) /
                    circuit_resistance_ohm;
                if (!value_within_constraint(peak_power_W, constraint)) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "resistor power dissipation V^2/R exceeds model validity envelope continuous_power");
                }
            }
        }

        // Check resolved parameter's own applicable_conditions against matching envelope ranges.
        for (const auto& cond : resolved_param.operating_conditions) {
            if (quantity_matches(cond.parameter_name, constraint.quantity_name) &&
                cond.numeric_value.has_value()) {
                if (cond.unit_symbol != constraint.unit_symbol) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "parameter operating condition unit '" + cond.unit_symbol +
                            "' does not match envelope unit '" + constraint.unit_symbol + "'");
                } else if (!value_within_constraint(*cond.numeric_value, constraint)) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "parameter operating condition '" + cond.parameter_name +
                            "' falls outside model validity envelope");
                }
            }
        }

        // Check additional caller-supplied operating conditions against matching envelope ranges.
        for (const auto& cond : environment.additional_conditions) {
            if (quantity_matches(cond.parameter_name, constraint.quantity_name)) {
                if (cond.unit_symbol != constraint.unit_symbol) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "additional operating condition unit '" + cond.unit_symbol +
                            "' does not match envelope unit '" + constraint.unit_symbol + "'");
                } else if (!cond.numeric_value.has_value() ||
                           !std::isfinite(*cond.numeric_value)) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "additional operating condition matching envelope constraint must provide a finite numeric_value");
                } else if (!value_within_constraint(*cond.numeric_value, constraint)) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "additional operating condition '" + cond.parameter_name +
                            "' falls outside model validity envelope");
                }
            }
        }
    }
}

} // namespace

ModelCompatibilityReport evaluate_rc_model_compatibility(
    const CircuitAssembly& assembly,
    const ResolvedRcParameters& resolved_parameters,
    component::AnalysisType requested_analysis,
    const OperatingEnvironment& environment,
    ExecutionPolicy policy) {
    ModelCompatibilityReport out;

    if (!is_analysis_supported_by_rc_backend(requested_analysis)) {
        out.validation.add(
            VerticalValidationCode::unsupported_backend_analysis,
            "requested_analysis",
            "existing ideal-RC backend supports only AnalysisType::transient");
    }

    if (!std::isfinite(environment.ambient_temperature_degC)) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.ambient_temperature_degC",
            "ambient_temperature_degC must be finite");
    }
    if (!std::isfinite(environment.stimulus_voltage_V) || environment.stimulus_voltage_V <= 0.0) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.stimulus_voltage_V",
            "stimulus_voltage_V must be finite and positive");
    }
    if (!std::isfinite(environment.initial_voltage_V)) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.initial_voltage_V",
            "initial_voltage_V must be finite");
    }

    for (std::size_t i = 0; i < environment.additional_conditions.size(); ++i) {
        const auto cond_report = component::validate_operating_condition(
            environment.additional_conditions[i],
            "environment.additional_conditions[" + std::to_string(i) + "]");
        if (!cond_report.valid()) {
            out.validation.add(
                VerticalValidationCode::invalid_operating_condition,
                "environment.additional_conditions[" + std::to_string(i) + "]",
                cond_report.issues.front().message);
        }
    }

    evaluate_single_model(
        out.validation,
        assembly,
        resolved_parameters.resistor,
        requested_analysis,
        environment,
        resolved_parameters.resistor.resolved_si_value,
        out.resistor_validation_status,
        out.resistor_validation_record_ids);

    evaluate_single_model(
        out.validation,
        assembly,
        resolved_parameters.capacitor,
        requested_analysis,
        environment,
        resolved_parameters.resistor.resolved_si_value,
        out.capacitor_validation_status,
        out.capacitor_validation_record_ids);

    // Evaluate synthetic vs engineering-approved classification across both components.
    bool any_synthetic = false;
    bool any_non_synthetic = false;

    for (const auto* resolved : {&resolved_parameters.resistor, &resolved_parameters.capacitor}) {
        if (resolved->component.is_synthetic) {
            any_synthetic = true;
        } else {
            any_non_synthetic = true;
        }
        const auto* bundle = find_bundle_for_component(assembly, resolved->component.id);
        if (bundle != nullptr) {
            const auto* model = find_model_in_bundle(*bundle, resolved->model_id);
            if (model != nullptr) {
                if (model->is_synthetic) {
                    any_synthetic = true;
                } else {
                    any_non_synthetic = true;
                }
            }
        }
    }

    if (any_synthetic && any_non_synthetic) {
        out.validation.add(
            VerticalValidationCode::inconsistent_synthetic_classification,
            "circuit_assembly[" + assembly.id.value + "]",
            "circuit mixes synthetic test fixtures and non-synthetic engineering components");
    }

    if (any_synthetic) {
        out.classification = ExecutionClassification::test_only_synthetic;
        if (policy == ExecutionPolicy::engineering_approved_only) {
            out.validation.add(
                VerticalValidationCode::synthetic_data_prohibited_in_engineering_mode,
                "execution_policy",
                "synthetic component or model fixtures cannot execute under ExecutionPolicy::engineering_approved_only");
        }
    } else {
        out.classification = ExecutionClassification::engineering_approved;
        if (policy == ExecutionPolicy::test_only_synthetic) {
            out.validation.add(
                VerticalValidationCode::inconsistent_synthetic_classification,
                "execution_policy",
                "ExecutionPolicy::test_only_synthetic requires synthetic component and model fixtures");
        }
    }

    return out;
}

} // namespace datasim::vertical

