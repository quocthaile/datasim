#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include <algorithm>
#include <set>
#include <string_view>

namespace datasim::vertical {

namespace {

bool is_non_blank_line(std::string_view text) noexcept {
    if (text.empty()) {
        return false;
    }
    bool has_visible = false;
    for (unsigned char uc : text) {
        if (uc < 0x20U || uc == 0x7FU) {
            return false;
        }
        if (uc != ' ') {
            has_visible = true;
        }
    }
    return has_visible;
}

void check_token(
    VerticalValidationReport& report,
    std::string_view token,
    const std::string& path,
    std::string_view label) {
    if (token.empty()) {
        report.add(
            VerticalValidationCode::empty_required_field,
            path,
            std::string(label) + " must not be empty");
    } else if (!component::is_valid_identifier_token(token)) {
        report.add(
            VerticalValidationCode::invalid_identifier,
            path,
            std::string(label) + " contains whitespace or control characters");
    }
}

void validate_instance_terminals(
    VerticalValidationReport& report,
    const CircuitAssembly& assembly,
    const CircuitComponentInstance& inst,
    const component::ModelArtifactDescriptor& model,
    const std::string& ipath) {
    if (inst.terminal_bindings.size() != model.terminals.size()) {
        report.add(
            VerticalValidationCode::invalid_terminal_node_binding,
            ipath + ".terminal_bindings",
            "instance terminal_bindings count must match selected model terminals count");
        return;
    }

    for (std::size_t i = 0; i < model.terminals.size(); ++i) {
        const auto& expected_term = model.terminals[i];
        const auto& binding = inst.terminal_bindings[i];
        const std::string bpath = ipath + ".terminal_bindings[" + std::to_string(i) + "]";

        if (binding.terminal_position != expected_term.position ||
            binding.terminal_name != expected_term.terminal_name ||
            binding.physical_pin_id != expected_term.physical_pin_id) {
            report.add(
                VerticalValidationCode::invalid_terminal_node_binding,
                bpath,
                "terminal binding must preserve exact model terminal position, name, and physical_pin_id");
        }
        if (binding.circuit_node_id.empty()) {
            report.add(
                VerticalValidationCode::empty_required_field,
                bpath + ".circuit_node_id",
                "circuit_node_id must not be empty");
        } else if (!component::is_valid_identifier_token(binding.circuit_node_id)) {
            report.add(
                VerticalValidationCode::invalid_identifier,
                bpath + ".circuit_node_id",
                "circuit_node_id contains invalid characters");
        }
    }

    if (inst.terminal_bindings.size() == 2U) {
        const auto& n0 = inst.terminal_bindings[0].circuit_node_id;
        const auto& n1 = inst.terminal_bindings[1].circuit_node_id;
        if (!n0.empty() && n0 == n1) {
            report.add(
                VerticalValidationCode::shorted_component_terminals,
                ipath + ".terminal_bindings",
                "two-terminal passive component terminals must not be shorted to the same circuit node");
            return;
        }
        const auto connects_pair = [&](const std::string& a, const std::string& b) {
            return (n0 == a && n1 == b) || (n0 == b && n1 == a);
        };
        if (inst.role == CircuitElementRole::series_resistor) {
            if (!connects_pair(assembly.input_node, assembly.output_node)) {
                report.add(
                    VerticalValidationCode::topology_connectivity_mismatch,
                    ipath + ".terminal_bindings",
                    "series_resistor terminals must connect circuit input_node and output_node");
            }
        } else if (inst.role == CircuitElementRole::shunt_capacitor) {
            if (!connects_pair(assembly.output_node, assembly.ground_node)) {
                report.add(
                    VerticalValidationCode::topology_connectivity_mismatch,
                    ipath + ".terminal_bindings",
                    "shunt_capacitor terminals must connect circuit output_node and ground_node");
            }
        }
    } else {
        report.add(
            VerticalValidationCode::invalid_terminal_node_binding,
            ipath + ".terminal_bindings",
            "series RC topology requires 2-terminal passive component models");
    }
}

} // namespace

const component::ComponentDomainBundle* find_bundle_for_component(
    const CircuitAssembly& assembly,
    const component::ComponentId& component_id) noexcept {
    for (const auto& bundle : assembly.component_bundles) {
        if (bundle.component.id == component_id) {
            return &bundle;
        }
    }
    return nullptr;
}

const component::ModelArtifactDescriptor* find_model_in_bundle(
    const component::ComponentDomainBundle& bundle,
    const component::ModelArtifactId& model_id) noexcept {
    for (const auto& model : bundle.models) {
        if (model.id == model_id) {
            return &model;
        }
    }
    return nullptr;
}

const CircuitComponentInstance* find_instance_by_role(
    const CircuitAssembly& assembly,
    CircuitElementRole role) noexcept {
    for (const auto& inst : assembly.instances) {
        if (inst.role == role) {
            return &inst;
        }
    }
    return nullptr;
}

VerticalValidationReport validate_circuit_assembly(const CircuitAssembly& assembly) {
    VerticalValidationReport report;
    const std::string base = "circuit_assembly[" + assembly.id.value + "]";

    check_token(report, assembly.id.value, base + ".id", "CircuitAssemblyId");
    if (!is_non_blank_line(assembly.name)) {
        report.add(
            VerticalValidationCode::empty_required_field,
            base + ".name",
            "circuit assembly name must be non-empty and single-line");
    }

    if (assembly.topology != CircuitTopologyKind::series_rc_lowpass) {
        report.add(
            VerticalValidationCode::unsupported_circuit_topology,
            base + ".topology",
            "only series_rc_lowpass topology is supported by the ideal-RC backend");
    }

    check_token(report, assembly.input_node, base + ".input_node", "input_node");
    check_token(report, assembly.output_node, base + ".output_node", "output_node");
    check_token(report, assembly.ground_node, base + ".ground_node", "ground_node");

    if (assembly.input_node != "vin" ||
        assembly.output_node != "vout" ||
        assembly.ground_node != "0") {
        report.add(
            VerticalValidationCode::invalid_circuit_nodes,
            base + ".nodes",
            "ideal-RC backend requires canonical nodes input_node='vin', output_node='vout', ground_node='0'");
    }

    if (assembly.component_bundles.empty()) {
        report.add(
            VerticalValidationCode::empty_required_field,
            base + ".component_bundles",
            "circuit assembly must contain component_bundles");
    }

    std::set<component::ComponentId> seen_bundle_components;
    for (std::size_t i = 0; i < assembly.component_bundles.size(); ++i) {
        const auto& bundle = assembly.component_bundles[i];
        const std::string bpath = base + ".component_bundles[" + std::to_string(i) + "]";

        if (!bundle.component.id.empty() && !seen_bundle_components.insert(bundle.component.id).second) {
            report.add(
                VerticalValidationCode::duplicate_identifier,
                bpath + ".component.id",
                "duplicate ComponentId in circuit assembly component_bundles: " + bundle.component.id.value);
        }

        const auto bundle_report = component::validate_domain_bundle(bundle);
        if (!bundle_report.valid()) {
            const auto& first_issue = bundle_report.issues.front();
            report.add(
                VerticalValidationCode::invalid_component_bundle,
                bpath + "." + first_issue.entity_path,
                "component bundle failed domain validation: " + first_issue.message);
        }
    }

    if (assembly.instances.empty()) {
        report.add(
            VerticalValidationCode::empty_required_field,
            base + ".instances",
            "circuit assembly must contain placed component instances");
    }

    std::set<ComponentInstanceId> seen_instances;
    std::size_t resistor_role_count = 0;
    std::size_t capacitor_role_count = 0;
    std::size_t other_role_count = 0;

    for (std::size_t i = 0; i < assembly.instances.size(); ++i) {
        const auto& inst = assembly.instances[i];
        const std::string ipath = base + ".instances[" + inst.instance_id.value + "]";

        check_token(report, inst.instance_id.value, ipath + ".instance_id", "ComponentInstanceId");
        if (!inst.instance_id.empty() && !seen_instances.insert(inst.instance_id).second) {
            report.add(
                VerticalValidationCode::duplicate_identifier,
                ipath + ".instance_id",
                "duplicate ComponentInstanceId in circuit assembly: " + inst.instance_id.value);
        }

        if (inst.role == CircuitElementRole::series_resistor) {
            ++resistor_role_count;
        } else if (inst.role == CircuitElementRole::shunt_capacitor) {
            ++capacitor_role_count;
        } else {
            ++other_role_count;
        }

        check_token(report, inst.component_id.value, ipath + ".component_id", "ComponentId");
        check_token(report, inst.knowledge_id.value, ipath + ".knowledge_id", "ComponentKnowledgeId");
        check_token(report, inst.selected_model_id.value, ipath + ".selected_model_id", "ModelArtifactId");
        if (inst.selected_model_version.empty()) {
            report.add(
                VerticalValidationCode::empty_required_field,
                ipath + ".selected_model_version",
                "selected_model_version must not be empty");
        } else if (!component::is_valid_version_token(inst.selected_model_version)) {
            report.add(
                VerticalValidationCode::invalid_identifier,
                ipath + ".selected_model_version",
                "selected_model_version contains invalid characters");
        }

        const auto* bundle = find_bundle_for_component(assembly, inst.component_id);
        if (bundle == nullptr) {
            report.add(
                VerticalValidationCode::unresolved_component_reference,
                ipath + ".component_id",
                "instance component_id does not resolve to any ComponentDomainBundle in assembly");
            continue;
        }

        if (inst.role == CircuitElementRole::series_resistor &&
            bundle->component.family != component::ComponentFamily::resistor) {
            report.add(
                VerticalValidationCode::component_family_role_mismatch,
                ipath + ".role",
                "series_resistor instance must reference a ComponentFamily::resistor component");
        } else if (inst.role == CircuitElementRole::shunt_capacitor &&
                   bundle->component.family != component::ComponentFamily::capacitor) {
            report.add(
                VerticalValidationCode::component_family_role_mismatch,
                ipath + ".role",
                "shunt_capacitor instance must reference a ComponentFamily::capacitor component");
        }

        if (!bundle->knowledge.has_value()) {
            report.add(
                VerticalValidationCode::unresolved_knowledge_reference,
                ipath + ".knowledge_id",
                "referenced component bundle contains no ComponentKnowledge");
        } else {
            const auto& kn = *bundle->knowledge;
            if (kn.id != inst.knowledge_id) {
                report.add(
                    VerticalValidationCode::unresolved_knowledge_reference,
                    ipath + ".knowledge_id",
                    "instance knowledge_id does not match bundle ComponentKnowledge::id");
            }
            if (kn.version != inst.expected_knowledge_version) {
                report.add(
                    VerticalValidationCode::knowledge_version_mismatch,
                    ipath + ".expected_knowledge_version",
                    "instance expected_knowledge_version does not match ComponentKnowledge::version");
            }
            if (kn.review_decision.new_state != component::ReviewState::approved) {
                report.add(
                    VerticalValidationCode::unapproved_component_knowledge,
                    ipath + ".knowledge_id",
                    "referenced ComponentKnowledge is not in ReviewState::approved");
            }
            if (kn.has_unresolved_conflicts()) {
                report.add(
                    VerticalValidationCode::unresolved_knowledge_conflicts,
                    ipath + ".knowledge_id",
                    "referenced ComponentKnowledge contains unresolved parameter conflicts");
            }
        }

        const auto* model = find_model_in_bundle(*bundle, inst.selected_model_id);
        if (model == nullptr) {
            report.add(
                VerticalValidationCode::unresolved_model_reference,
                ipath + ".selected_model_id",
                "selected_model_id does not resolve to any ModelArtifactDescriptor in component bundle");
            continue;
        }

        if (model->version != inst.selected_model_version) {
            report.add(
                VerticalValidationCode::model_version_mismatch,
                ipath + ".selected_model_version",
                "selected_model_version does not match ModelArtifactDescriptor::version");
        }

        if (!model->component_knowledge_id.has_value() ||
            !bundle->knowledge.has_value() ||
            *model->component_knowledge_id != bundle->knowledge->id) {
            report.add(
                VerticalValidationCode::model_knowledge_mismatch,
                ipath + ".selected_model_id",
                "selected ModelArtifactDescriptor must explicitly bind the bundle's approved ComponentKnowledgeId");
        }

        validate_instance_terminals(report, assembly, inst, *model, ipath);
    }

    if (resistor_role_count != 1U || capacitor_role_count != 1U || other_role_count != 0U ||
        assembly.instances.size() != 2U) {
        report.add(
            VerticalValidationCode::invalid_topology_element_roles,
            base + ".instances",
            "series_rc_lowpass requires exactly one series_resistor instance and one shunt_capacitor instance");
    }

    return report;
}

} // namespace datasim::vertical
