#include "datasim/vertical/compatibility/model_compatibility.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace datasim::vertical {

namespace {

bool constraint_is_well_formed(const component::NumericRangeConstraint& constraint) noexcept {
    if (constraint.quantity_name.empty() || constraint.unit_symbol.empty()) {
        return false;
    }
    if (!constraint.min_value.has_value() && !constraint.max_value.has_value()) {
        return false;
    }
    if (constraint.min_value.has_value() && !std::isfinite(*constraint.min_value)) {
        return false;
    }
    if (constraint.max_value.has_value() && !std::isfinite(*constraint.max_value)) {
        return false;
    }
    if (constraint.min_value.has_value() && constraint.max_value.has_value()) {
        if (*constraint.min_value > *constraint.max_value) {
            return false;
        }
        if (*constraint.min_value == *constraint.max_value &&
            (!constraint.min_inclusive || !constraint.max_inclusive)) {
            return false;
        }
    }
    return true;
}

bool value_within_constraint(
    double value,
    const component::NumericRangeConstraint& constraint) noexcept {
    if (!std::isfinite(value) || !constraint_is_well_formed(constraint)) {
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

std::string_view canonical_condition_quantity(std::string_view cond_name) noexcept {
    if (cond_name == "ambient_temperature" || cond_name == "T_A" || cond_name == "T_ambient") {
        return "ambient_temperature";
    }
    if (cond_name == "terminal_dc_voltage" || cond_name == "V_DC" ||
        cond_name == "V_terminal" || cond_name == "voltage_V") {
        return "terminal_dc_voltage";
    }
    if (cond_name == "continuous_power" || cond_name == "P_load" ||
        cond_name == "P_continuous" || cond_name == "power_W") {
        return "continuous_power";
    }
    if (cond_name == "signal_frequency" || cond_name == "f_signal" ||
        cond_name == "frequency_Hz") {
        return "signal_frequency";
    }
    return cond_name;
}

bool quantity_matches(std::string_view cond_name, std::string_view constraint_name) noexcept {
    return canonical_condition_quantity(cond_name) == constraint_name;
}

void validate_condition_list_consistency(
    VerticalValidationReport& report,
    const std::vector<component::OperatingCondition>& conditions,
    const std::string& base_path,
    bool require_finite_numeric) {
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        const auto& cond = conditions[i];
        const std::string cpath = base_path + "[" + std::to_string(i) + "]";
        const auto cond_report = component::validate_operating_condition(cond, cpath);
        if (!cond_report.valid()) {
            report.add(
                VerticalValidationCode::invalid_operating_condition,
                cpath,
                cond_report.issues.front().message);
        } else if (require_finite_numeric &&
                   (!cond.numeric_value.has_value() || !std::isfinite(*cond.numeric_value))) {
            report.add(
                VerticalValidationCode::invalid_operating_condition,
                cpath + ".numeric_value",
                "operating condition '" + cond.parameter_name +
                    "' must provide a finite numeric_value");
        }

        const std::string_view canon_i = canonical_condition_quantity(cond.parameter_name);
        for (std::size_t j = 0; j < i; ++j) {
            const auto& prev = conditions[j];
            if (canonical_condition_quantity(prev.parameter_name) != canon_i) {
                continue;
            }
            if (cond.unit_symbol != prev.unit_symbol) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "duplicate operating conditions for '" + std::string(canon_i) +
                        "' have conflicting units ('" + prev.unit_symbol + "' vs '" +
                        cond.unit_symbol + "')");
            } else if (!cond.numeric_value.has_value() ||
                       !prev.numeric_value.has_value() ||
                       !std::isfinite(*cond.numeric_value) ||
                       !std::isfinite(*prev.numeric_value)) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "duplicate operating conditions for '" + std::string(canon_i) +
                        "' must both provide finite numeric values");
            } else if (*cond.numeric_value != *prev.numeric_value) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "conflicting duplicate operating conditions for '" + std::string(canon_i) + "'");
            }
        }
    }
}

bool is_model_validation_record_usable(
    const component::ComponentDomainBundle& bundle,
    const component::ModelArtifactDescriptor& model,
    const component::ModelValidationRecord& rec) {
    if (!component::validate_model_validation_record(rec).valid()) {
        return false;
    }
    for (const auto& ev_id : rec.evidence_refs) {
        const bool found_ev = std::any_of(
            bundle.evidence_locations.begin(),
            bundle.evidence_locations.end(),
            [&](const component::EvidenceLocation& ev) { return ev.id == ev_id; });
        if (!found_ev) {
            return false;
        }
    }
    for (const auto& analysis : rec.verified_analyses) {
        if (std::find(
                model.supported_analyses.begin(),
                model.supported_analyses.end(),
                analysis) == model.supported_analyses.end()) {
            return false;
        }
    }
    return true;
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
        report.add(
            VerticalValidationCode::unresolved_component_reference,
            mpath,
            "resolved parameter component_id does not resolve in circuit assembly");
        return;
    }
    const auto* model = find_model_in_bundle(*bundle, resolved_param.model_id);
    if (model == nullptr) {
        report.add(
            VerticalValidationCode::unresolved_model_reference,
            mpath,
            "resolved parameter model_id does not resolve in component bundle");
        return;
    }

    if (!bundle->knowledge.has_value()) {
        report.add(
            VerticalValidationCode::unresolved_knowledge_reference,
            mpath + ".component_knowledge_id",
            "component bundle contains no ComponentKnowledge");
    } else {
        const auto& kn = *bundle->knowledge;
        if (kn.review_decision.new_state != component::ReviewState::approved ||
            !component::validate_review_decision(kn.review_decision).valid()) {
            report.add(
                VerticalValidationCode::unapproved_component_knowledge,
                mpath + ".component_knowledge_id",
                "referenced ComponentKnowledge is not approved by a valid human_engineer review decision");
        }
        if (!model->component_knowledge_id.has_value() || *model->component_knowledge_id != kn.id) {
            report.add(
                VerticalValidationCode::model_knowledge_mismatch,
                mpath + ".component_knowledge_id",
                "selected ModelArtifactDescriptor must bind the bundle's approved ComponentKnowledgeId");
        }
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

    // Inspect validation records for existence, structural/reviewer validity, version matches, and effective status.
    std::vector<component::ModelValidationRecord> usable_validation_records;
    for (const auto& ref_id : model->validation_record_refs) {
        const component::ModelValidationRecord* matched_rec = nullptr;
        for (const auto& rec : bundle->model_validations) {
            if (rec.id == ref_id && rec.model_id == model->id) {
                matched_rec = &rec;
                break;
            }
        }
        if (matched_rec == nullptr) {
            report.add(
                VerticalValidationCode::invalid_component_bundle,
                mpath + ".validation_record_refs",
                "ModelArtifactDescriptor references unknown or mismatched ModelValidationId '" +
                    ref_id.value + "'");
            continue;
        }
        if (matched_rec->model_version != model->version) {
            report.add(
                VerticalValidationCode::model_version_mismatch,
                mpath + ".validation_record[" + matched_rec->id.value + "].model_version",
                "ModelValidationRecord version '" + matched_rec->model_version +
                    "' does not match selected ModelArtifactDescriptor version '" +
                    model->version + "'");
        }
        if (!is_model_validation_record_usable(*bundle, *model, *matched_rec)) {
            report.add(
                VerticalValidationCode::invalid_component_bundle,
                mpath + ".validation_record[" + matched_rec->id.value + "]",
                "ModelValidationRecord failed domain validation (requires human_engineer reviewer, timestamp, verified_analyses, and valid evidence_refs)");
            continue;
        }
        usable_validation_records.push_back(*matched_rec);
    }

    out_status = component::effective_model_validation_status(*model, usable_validation_records);
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
    for (const auto& rec : usable_validation_records) {
        if (rec.model_id == model->id &&
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

    if (out_status == component::ModelValidationStatus::validated_within_envelope &&
        !analysis_verified_by_record) {
        report.add(
            VerticalValidationCode::analysis_not_verified_in_validation_record,
            mpath + ".verified_analyses",
            "no current validated_within_envelope ModelValidationRecord verifies the requested analysis");
    }

    validate_condition_list_consistency(
        report,
        resolved_param.operating_conditions,
        mpath + ".resolved_parameter.operating_conditions",
        false);

    if (model->validity_envelope.operating_ranges.empty()) {
        report.add(
            VerticalValidationCode::invalid_operating_condition,
            mpath + ".validity_envelope.operating_ranges",
            "ModelValidityEnvelope must declare at least one operating range constraint");
        return;
    }

    // Fail-closed evaluation of every constraint in the model validity envelope.
    for (std::size_t ci = 0; ci < model->validity_envelope.operating_ranges.size(); ++ci) {
        const auto& constraint = model->validity_envelope.operating_ranges[ci];
        const std::string cpath =
            mpath + ".validity_envelope[" + constraint.quantity_name + "]";

        if (!constraint_is_well_formed(constraint)) {
            report.add(
                VerticalValidationCode::invalid_operating_condition,
                cpath,
                "validity envelope constraint is malformed (empty name/unit, non-finite bound, or inverted range)");
            continue;
        }

        if (constraint.quantity_name == "ambient_temperature") {
            if (constraint.unit_symbol != "degC") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "ambient_temperature validity envelope constraint must use 'degC'");
            } else if (!environment.ambient_temperature_degC.has_value()) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "missing required runtime ambient_temperature_degC for ambient_temperature validity envelope constraint");
            } else if (!std::isfinite(*environment.ambient_temperature_degC)) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "runtime ambient_temperature_degC must be finite");
            } else if (!value_within_constraint(*environment.ambient_temperature_degC, constraint)) {
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
            } else if (!environment.stimulus_voltage_V.has_value() ||
                       !environment.initial_voltage_V.has_value()) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "missing required runtime terminal voltage (stimulus_voltage_V / initial_voltage_V) for terminal_dc_voltage validity envelope constraint");
            } else if (!std::isfinite(*environment.stimulus_voltage_V) ||
                       !std::isfinite(*environment.initial_voltage_V)) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "runtime terminal voltages must be finite");
            } else {
                const double v_stim = *environment.stimulus_voltage_V;
                const double v_init = *environment.initial_voltage_V;
                const double v_step_diff = v_stim - v_init;
                const bool diff_ok =
                    (resolved_param.role != CircuitElementRole::series_resistor) ||
                    value_within_constraint(v_step_diff, constraint);
                if (!value_within_constraint(v_stim, constraint) ||
                    !value_within_constraint(v_init, constraint) ||
                    !diff_ok) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "circuit terminal voltage falls outside model validity envelope");
                }
            }
        } else if (constraint.quantity_name == "continuous_power") {
            if (resolved_param.role != CircuitElementRole::series_resistor) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "continuous_power validity envelope constraint is only supported for series_resistor models in the ideal-RC backend");
                continue;
            }
            if (constraint.unit_symbol != "W") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "continuous_power validity envelope constraint must use 'W'");
            } else if (!environment.stimulus_voltage_V.has_value() ||
                       !environment.initial_voltage_V.has_value()) {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "missing required runtime terminal voltage for conservative continuous_power screening");
            } else {
                const double peak_power_W = conservative_peak_resistor_power_W(
                    *environment.stimulus_voltage_V,
                    *environment.initial_voltage_V,
                    circuit_resistance_ohm);
                if (!std::isfinite(peak_power_W)) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "non-finite terminal voltage or invalid resistance prevents conservative continuous_power screening");
                } else if (!value_within_constraint(peak_power_W, constraint)) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "conservative peak resistor power dissipation exceeds model validity envelope continuous_power");
                }
            }
        } else if (constraint.quantity_name == "signal_frequency") {
            if (constraint.unit_symbol != "Hz") {
                report.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "signal_frequency validity envelope constraint must use 'Hz'");
            } else {
                const bool has_explicit_runtime_freq = std::any_of(
                    environment.additional_conditions.begin(),
                    environment.additional_conditions.end(),
                    [&](const component::OperatingCondition& cond) {
                        return quantity_matches(cond.parameter_name, constraint.quantity_name);
                    });
                if (!has_explicit_runtime_freq) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "missing required runtime signal_frequency operating condition in environment.additional_conditions; transient pulse timing does not imply a single equivalent signal frequency");
                }
            }
        } else {
            report.add(
                VerticalValidationCode::invalid_operating_condition,
                cpath,
                "unsupported validity envelope constraint quantity '" +
                    constraint.quantity_name + "' cannot be evaluated by the ideal-RC backend");
            continue;
        }

        // Check resolved parameter's own applicable_conditions against matching envelope ranges.
        // Fail closed if a matching condition omits a finite numeric_value or uses a mismatched unit.
        for (const auto& cond : resolved_param.operating_conditions) {
            if (quantity_matches(cond.parameter_name, constraint.quantity_name)) {
                if (cond.unit_symbol != constraint.unit_symbol) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "parameter operating condition unit '" + cond.unit_symbol +
                            "' does not match envelope unit '" + constraint.unit_symbol + "'");
                } else if (!cond.numeric_value.has_value() ||
                           !std::isfinite(*cond.numeric_value)) {
                    report.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "parameter operating condition '" + cond.parameter_name +
                            "' matching validity envelope constraint must provide a finite numeric_value");
                } else if (!value_within_constraint(*cond.numeric_value, constraint)) {
                    report.add(
                        VerticalValidationCode::operating_condition_outside_validity_envelope,
                        cpath,
                        "parameter operating condition '" + cond.parameter_name +
                            "' falls outside model validity envelope");
                }
            }
        }

        // Check caller-supplied additional operating conditions against matching envelope ranges.
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

void inspect_component_synthetic_markers(
    const CircuitAssembly& assembly,
    const ResolvedComponentParameter& resolved,
    bool& any_synthetic,
    bool& any_non_synthetic) {
    if (resolved.component.is_synthetic) {
        any_synthetic = true;
    } else {
        any_non_synthetic = true;
    }

    for (const auto& doc : resolved.source_documents) {
        if (doc.is_synthetic) {
            any_synthetic = true;
        } else {
            any_non_synthetic = true;
        }
    }

    const auto* bundle = find_bundle_for_component(assembly, resolved.component.id);
    if (bundle == nullptr) {
        return;
    }

    if (bundle->component.is_synthetic) {
        any_synthetic = true;
    } else {
        any_non_synthetic = true;
    }

    if (bundle->knowledge.has_value()) {
        if (bundle->knowledge->component.is_synthetic) {
            any_synthetic = true;
        } else {
            any_non_synthetic = true;
        }
    }

    for (const auto& doc : bundle->documents) {
        if (doc.is_synthetic) {
            any_synthetic = true;
        } else {
            any_non_synthetic = true;
        }
    }

    const bool synthetic_source_context =
        bundle->component.is_synthetic &&
        !bundle->documents.empty() &&
        std::all_of(
            bundle->documents.begin(),
            bundle->documents.end(),
            [](const component::SourceDocument& doc) { return doc.is_synthetic; });

    for (const auto& obs : bundle->observations) {
        if (obs.id == resolved.source_observation_id ||
            (bundle->knowledge.has_value() &&
             std::find(
                 bundle->knowledge->approved_observation_refs.begin(),
                 bundle->knowledge->approved_observation_refs.end(),
                 obs.id) != bundle->knowledge->approved_observation_refs.end())) {
            if (obs.extraction_method == component::ExtractionMethod::synthetic_fixture ||
                (synthetic_source_context &&
                 obs.extraction_method == component::ExtractionMethod::deterministic_table_parser)) {
                any_synthetic = true;
            } else {
                any_non_synthetic = true;
            }
        }
    }

    const auto* model = find_model_in_bundle(*bundle, resolved.model_id);
    if (model != nullptr) {
        if (model->is_synthetic) {
            any_synthetic = true;
        } else {
            any_non_synthetic = true;
        }
    }
}

} // namespace

double conservative_peak_resistor_power_W(
    double stimulus_voltage_V,
    double initial_voltage_V,
    double resistance_ohm) noexcept {
    if (!std::isfinite(stimulus_voltage_V) ||
        !std::isfinite(initial_voltage_V) ||
        !std::isfinite(resistance_ohm) ||
        resistance_ohm <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double v_stim_sq = stimulus_voltage_V * stimulus_voltage_V;
    const double v_init_sq = initial_voltage_V * initial_voltage_V;
    const double v_step_diff = stimulus_voltage_V - initial_voltage_V;
    const double v_diff_sq = v_step_diff * v_step_diff;
    return std::max({v_stim_sq, v_init_sq, v_diff_sq}) / resistance_ohm;
}

bool is_supported_rc_envelope_quantity(
    std::string_view quantity_name,
    CircuitElementRole role) noexcept {
    if (quantity_name == "ambient_temperature" ||
        quantity_name == "terminal_dc_voltage" ||
        quantity_name == "signal_frequency") {
        return role == CircuitElementRole::series_resistor ||
               role == CircuitElementRole::shunt_capacitor;
    }
    if (quantity_name == "continuous_power") {
        return role == CircuitElementRole::series_resistor;
    }
    return false;
}

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

    if (environment.ambient_temperature_degC.has_value() &&
        !std::isfinite(*environment.ambient_temperature_degC)) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.ambient_temperature_degC",
            "ambient_temperature_degC must be finite");
    }
    if (environment.stimulus_voltage_V.has_value() &&
        (!std::isfinite(*environment.stimulus_voltage_V) || *environment.stimulus_voltage_V <= 0.0)) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.stimulus_voltage_V",
            "stimulus_voltage_V must be finite and positive");
    }
    if (environment.initial_voltage_V.has_value() &&
        !std::isfinite(*environment.initial_voltage_V)) {
        out.validation.add(
            VerticalValidationCode::invalid_operating_condition,
            "environment.initial_voltage_V",
            "initial_voltage_V must be finite");
    }

    validate_condition_list_consistency(
        out.validation,
        environment.additional_conditions,
        "environment.additional_conditions",
        true);

    // Check additional_conditions against primary OperatingEnvironment fields to prevent
    // silent precedence or conflicting runtime specifications for the same physical quantity.
    for (std::size_t i = 0; i < environment.additional_conditions.size(); ++i) {
        const auto& cond = environment.additional_conditions[i];
        const std::string cpath =
            "environment.additional_conditions[" + std::to_string(i) + "]";
        const std::string_view canon = canonical_condition_quantity(cond.parameter_name);
        if (!cond.numeric_value.has_value() || !std::isfinite(*cond.numeric_value)) {
            continue;
        }
        if (canon == "ambient_temperature") {
            if (cond.unit_symbol != "degC") {
                out.validation.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath + ".unit_symbol",
                    "ambient_temperature operating condition must use 'degC'");
            } else if (environment.ambient_temperature_degC.has_value() &&
                       std::isfinite(*environment.ambient_temperature_degC) &&
                       *cond.numeric_value != *environment.ambient_temperature_degC) {
                out.validation.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "additional operating condition for ambient_temperature conflicts with environment.ambient_temperature_degC");
            }
        } else if (canon == "terminal_dc_voltage") {
            if (cond.unit_symbol != "V") {
                out.validation.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath + ".unit_symbol",
                    "terminal_dc_voltage operating condition must use 'V'");
            } else if (environment.stimulus_voltage_V.has_value() &&
                       std::isfinite(*environment.stimulus_voltage_V) &&
                       *cond.numeric_value != *environment.stimulus_voltage_V) {
                out.validation.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath,
                    "additional operating condition for terminal_dc_voltage conflicts with environment.stimulus_voltage_V");
            }
        } else if (canon == "continuous_power") {
            if (cond.unit_symbol != "W") {
                out.validation.add(
                    VerticalValidationCode::invalid_operating_condition,
                    cpath + ".unit_symbol",
                    "continuous_power operating condition must use 'W'");
            } else if (environment.stimulus_voltage_V.has_value() &&
                       environment.initial_voltage_V.has_value()) {
                const double peak_p = conservative_peak_resistor_power_W(
                    *environment.stimulus_voltage_V,
                    *environment.initial_voltage_V,
                    resolved_parameters.resistor.resolved_si_value);
                if (std::isfinite(peak_p) && std::abs(*cond.numeric_value - peak_p) > 1e-12) {
                    out.validation.add(
                        VerticalValidationCode::invalid_operating_condition,
                        cpath,
                        "additional operating condition for continuous_power conflicts with conservative resistor peak power");
                }
            }
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

    // Evaluate synthetic vs non-synthetic classification across all component, document,
    // observation extraction_method, knowledge, and model metadata records.
    bool any_synthetic = false;
    bool any_non_synthetic = false;

    inspect_component_synthetic_markers(
        assembly, resolved_parameters.resistor, any_synthetic, any_non_synthetic);
    inspect_component_synthetic_markers(
        assembly, resolved_parameters.capacitor, any_synthetic, any_non_synthetic);

    if (any_synthetic && any_non_synthetic) {
        out.validation.add(
            VerticalValidationCode::inconsistent_synthetic_classification,
            "circuit_assembly[" + assembly.id.value + "]",
            "circuit mixes synthetic test fixtures and non-synthetic component/document/observation/model metadata");
    }

    if (any_synthetic) {
        out.classification = ExecutionClassification::test_only_synthetic;
        if (policy == ExecutionPolicy::engineering_approved_only) {
            out.validation.add(
                VerticalValidationCode::synthetic_data_prohibited_in_engineering_mode,
                "execution_policy",
                "synthetic component, document, observation, or model fixtures cannot execute under ExecutionPolicy::engineering_approved_only");
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
