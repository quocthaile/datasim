#include "datasim/vertical/resolution/parameter_resolution.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <string_view>

namespace datasim::vertical {

namespace {

bool is_supported_direct_mapping_expression(
    std::string_view model_param_name,
    const std::optional<std::string>& mapping_expression) noexcept {
    if (!mapping_expression.has_value()) {
        return true;
    }
    const std::string_view expr = *mapping_expression;
    if (expr.empty()) {
        return false;
    }
    if (model_param_name == "R") {
        return expr == "V(p1,p2) = R * I(p1,p2)" ||
               expr == "R = dc_resistance" ||
               expr == "R = dc_resistance(typical)" ||
               expr == "identity";
    }
    if (model_param_name == "C") {
        return expr == "I(p1,p2) = C * dV(p1,p2)/dt" ||
               expr == "I(port_a,port_b) = C * dV(port_a,port_b)/dt" ||
               expr == "C = capacitance" ||
               expr == "C = capacitance(typical)" ||
               expr == "identity";
    }
    return false;
}

std::optional<ResolvedComponentParameter> resolve_single_element_parameter(
    VerticalValidationReport& report,
    const CircuitAssembly& assembly,
    CircuitElementRole role,
    std::string_view expected_model_param_name,
    std::string_view expected_canonical_name,
    std::string_view required_si_unit) {
    const auto* inst = find_instance_by_role(assembly, role);
    if (inst == nullptr) {
        return std::nullopt;
    }

    const std::string ipath = "circuit_assembly[" + assembly.id.value +
                              "].instances[" + inst->instance_id.value + "]";

    const auto* bundle = find_bundle_for_component(assembly, inst->component_id);
    if (bundle == nullptr || !bundle->knowledge.has_value()) {
        return std::nullopt;
    }
    const auto& kn = *bundle->knowledge;
    const auto* model = find_model_in_bundle(*bundle, inst->selected_model_id);
    if (model == nullptr) {
        return std::nullopt;
    }

    const component::ModelParameterMapping* selected_mapping = nullptr;
    std::size_t matching_mapping_count = 0;

    for (std::size_t i = 0; i < model->parameter_mappings.size(); ++i) {
        const auto& m = model->parameter_mappings[i];
        const std::string mpath = ipath + ".model.parameter_mappings[" + std::to_string(i) + "]";
        if (m.model_parameter_name == expected_model_param_name) {
            ++matching_mapping_count;
            selected_mapping = &m;
        } else {
            report.add(
                VerticalValidationCode::unsupported_model_parameter,
                mpath,
                "ideal-RC backend does not support model parameter '" + m.model_parameter_name +
                    "'; expected only '" + std::string(expected_model_param_name) + "'");
        }
    }

    if (matching_mapping_count == 0U || selected_mapping == nullptr) {
        report.add(
            VerticalValidationCode::missing_model_parameter_mapping,
            ipath + ".model.parameter_mappings",
            "selected model is missing required parameter mapping for '" +
                std::string(expected_model_param_name) + "'");
        return std::nullopt;
    }

    if (matching_mapping_count > 1U) {
        report.add(
            VerticalValidationCode::conflicting_model_parameter_mappings,
            ipath + ".model.parameter_mappings",
            "selected model has multiple conflicting parameter mappings for '" +
                std::string(expected_model_param_name) + "'");
    }

    if (!selected_mapping->knowledge_param_id.has_value() ||
        selected_mapping->knowledge_param_id->empty()) {
        report.add(
            VerticalValidationCode::missing_model_parameter_mapping,
            ipath + ".model.parameter_mappings",
            "model parameter '" + std::string(expected_model_param_name) +
                "' must bind a ComponentKnowledge NormalizedParameterId rather than a fixed literal");
        return std::nullopt;
    }

    const auto& target_param_id = *selected_mapping->knowledge_param_id;
    const component::NormalizedParameter* resolved_np = nullptr;
    for (const auto& np : kn.normalized_parameters) {
        if (np.id == target_param_id) {
            resolved_np = &np;
            break;
        }
    }

    if (resolved_np == nullptr) {
        report.add(
            VerticalValidationCode::unresolved_normalized_parameter,
            ipath + ".knowledge.normalized_parameters",
            "NormalizedParameterId '" + target_param_id.value +
                "' does not exist in ComponentKnowledge");
        return std::nullopt;
    }

    // Check for conflicting typical normalized parameters of the same canonical quantity.
    std::size_t typical_same_canonical_count = 0;
    for (const auto& np : kn.normalized_parameters) {
        if (np.canonical_name == expected_canonical_name &&
            np.qualifier == component::ValueQualifier::typical) {
            ++typical_same_canonical_count;
        }
    }
    if (typical_same_canonical_count > 1U) {
        report.add(
            VerticalValidationCode::conflicting_normalized_parameters,
            ipath + ".knowledge.normalized_parameters",
            "ComponentKnowledge contains multiple conflicting typical normalized parameters for '" +
                std::string(expected_canonical_name) + "'");
    }

    if (resolved_np->canonical_name != expected_canonical_name) {
        report.add(
            VerticalValidationCode::non_si_or_incompatible_parameter_unit,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "].canonical_name",
            "expected canonical parameter '" + std::string(expected_canonical_name) +
                "', got '" + resolved_np->canonical_name + "'");
    }

    if (resolved_np->qualifier != component::ValueQualifier::typical) {
        report.add(
            VerticalValidationCode::non_typical_parameter_qualifier,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "].qualifier",
            "parameter resolution requires ValueQualifier::typical for nominal RC simulation");
    }

    if (resolved_np->category != component::ParameterCategory::electrical_characteristic) {
        report.add(
            VerticalValidationCode::invalid_parameter_category,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "].category",
            "parameter resolution requires ParameterCategory::electrical_characteristic");
    }

    if (!resolved_np->normalized_numeric_value.has_value() ||
        !std::isfinite(*resolved_np->normalized_numeric_value) ||
        *resolved_np->normalized_numeric_value <= 0.0) {
        report.add(
            VerticalValidationCode::missing_or_invalid_si_value,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "].normalized_numeric_value",
            "normalized_numeric_value must be present, finite, and strictly positive");
    }

    if (resolved_np->unitless ||
        !resolved_np->normalized_si_unit.has_value() ||
        *resolved_np->normalized_si_unit != required_si_unit) {
        report.add(
            VerticalValidationCode::non_si_or_incompatible_parameter_unit,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "].normalized_si_unit",
            "normalized_si_unit must be canonical SI '" + std::string(required_si_unit) + "'");
    }

    if (!component::are_parameter_units_compatible(
            selected_mapping->expected_unit,
            *resolved_np,
            selected_mapping->mapping_expression.has_value()
                ? std::optional<std::string_view>{*selected_mapping->mapping_expression}
                : std::nullopt)) {
        report.add(
            VerticalValidationCode::non_si_or_incompatible_parameter_unit,
            ipath + ".model.parameter_mappings",
            "model expected_unit '" + selected_mapping->expected_unit +
                "' is incompatible with NormalizedParameter unit");
    }

    if (selected_mapping->expected_unit != required_si_unit) {
        report.add(
            VerticalValidationCode::unsupported_unit_mapping,
            ipath + ".model.parameter_mappings.expected_unit",
            "model expected_unit must be canonical SI '" + std::string(required_si_unit) +
                "'; runtime unit scaling in ModelParameterMapping is not supported");
    }

    if (!is_supported_direct_mapping_expression(
            expected_model_param_name, selected_mapping->mapping_expression)) {
        report.add(
            VerticalValidationCode::unsupported_unit_mapping,
            ipath + ".model.parameter_mappings.mapping_expression",
            "unsupported model parameter mapping_expression; unit conversion must reside in NormalizedParameter::conversion_rule");
    }

    if (resolved_np->conversion_rule.empty() ||
        resolved_np->preserved_original_value_text.empty() ||
        !resolved_np->preserved_original_unit_symbol.has_value() ||
        resolved_np->preserved_original_unit_symbol->empty()) {
        report.add(
            VerticalValidationCode::missing_conversion_rule,
            ipath + ".knowledge.normalized_parameters[" + resolved_np->id.value + "]",
            "NormalizedParameter must preserve original value text, original unit symbol, and explicit conversion_rule");
    }

    for (std::size_t i = 0; i < resolved_np->applicable_conditions.size(); ++i) {
        const auto cond_report = component::validate_operating_condition(
            resolved_np->applicable_conditions[i],
            ipath + ".applicable_conditions[" + std::to_string(i) + "]");
        if (!cond_report.valid()) {
            report.add(
                VerticalValidationCode::invalid_operating_condition,
                ipath + ".applicable_conditions[" + std::to_string(i) + "]",
                cond_report.issues.front().message);
        }
    }

    const component::ParameterObservation* source_obs = nullptr;
    for (const auto& obs : bundle->observations) {
        if (obs.id == resolved_np->source_observation_id) {
            source_obs = &obs;
            break;
        }
    }

    if (source_obs == nullptr) {
        report.add(
            VerticalValidationCode::unresolved_source_observation,
            ipath + ".source_observation_id",
            "source_observation_id '" + resolved_np->source_observation_id.value +
                "' does not resolve to any ParameterObservation in bundle");
        return std::nullopt;
    }

    if (source_obs->review_state != component::ReviewState::approved) {
        report.add(
            VerticalValidationCode::unapproved_source_observation,
            ipath + ".source_observation_id",
            "source ParameterObservation is not in ReviewState::approved");
    }

    if (source_obs->evidence_refs.empty()) {
        report.add(
            VerticalValidationCode::unresolved_source_evidence,
            ipath + ".source_observation.evidence_refs",
            "source ParameterObservation contains no EvidenceId references");
    }

    std::vector<component::EvidenceLocation> evidence_locations;
    std::vector<component::SourceDocument> source_documents;
    std::set<component::SourceDocumentId> seen_doc_ids;

    for (const auto& ev_id : source_obs->evidence_refs) {
        const component::EvidenceLocation* found_ev = nullptr;
        for (const auto& ev : bundle->evidence_locations) {
            if (ev.id == ev_id) {
                found_ev = &ev;
                break;
            }
        }
        if (found_ev == nullptr) {
            report.add(
                VerticalValidationCode::unresolved_source_evidence,
                ipath + ".source_observation.evidence_refs",
                "EvidenceId '" + ev_id.value + "' does not resolve in bundle");
            continue;
        }
        evidence_locations.push_back(*found_ev);

        const component::SourceDocument* found_doc = nullptr;
        for (const auto& doc : bundle->documents) {
            if (doc.id == found_ev->source_document_id) {
                found_doc = &doc;
                break;
            }
        }
        if (found_doc == nullptr) {
            report.add(
                VerticalValidationCode::unresolved_source_document,
                ipath + ".source_evidence.source_document_id",
                "SourceDocumentId '" + found_ev->source_document_id.value + "' does not resolve in bundle");
            continue;
        }
        if (seen_doc_ids.insert(found_doc->id).second) {
            source_documents.push_back(*found_doc);
        }
    }

    if (!report.valid()) {
        return std::nullopt;
    }

    ResolvedComponentParameter out;
    out.instance_id = inst->instance_id;
    out.role = inst->role;
    out.component = bundle->component;
    out.knowledge_id = kn.id;
    out.knowledge_version = kn.version;
    out.model_id = model->id;
    out.model_version = model->version;
    out.model_parameter_name = std::string(expected_model_param_name);
    out.normalized_parameter_id = resolved_np->id;
    out.canonical_name = resolved_np->canonical_name;
    out.qualifier = resolved_np->qualifier;
    out.category = resolved_np->category;
    out.resolved_si_value = *resolved_np->normalized_numeric_value;
    out.normalized_si_unit = *resolved_np->normalized_si_unit;
    out.source_observation_id = resolved_np->source_observation_id;
    out.preserved_original_value_text = resolved_np->preserved_original_value_text;
    out.preserved_original_unit_symbol = resolved_np->preserved_original_unit_symbol.value_or("");
    out.conversion_rule = resolved_np->conversion_rule;
    out.model_mapping_expression = selected_mapping->mapping_expression;
    out.operating_conditions = resolved_np->applicable_conditions;
    out.source_evidence_locations = std::move(evidence_locations);
    out.source_documents = std::move(source_documents);
    out.terminal_bindings = inst->terminal_bindings;
    return out;
}

} // namespace

ParameterResolutionResult resolve_rc_parameters(const CircuitAssembly& assembly) {
    ParameterResolutionResult result;
    result.validation = validate_circuit_assembly(assembly);

    const auto res_param = resolve_single_element_parameter(
        result.validation,
        assembly,
        CircuitElementRole::series_resistor,
        "R",
        "dc_resistance",
        "ohm");

    const auto cap_param = resolve_single_element_parameter(
        result.validation,
        assembly,
        CircuitElementRole::shunt_capacitor,
        "C",
        "capacitance",
        "F");

    if (result.validation.valid() && res_param.has_value() && cap_param.has_value()) {
        result.parameters = ResolvedRcParameters{*res_param, *cap_param};
    }
    return result;
}

} // namespace datasim::vertical

