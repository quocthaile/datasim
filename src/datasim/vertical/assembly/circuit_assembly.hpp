#pragma once
#include "datasim/component/identity/ids.hpp"
#include "datasim/component/validation/domain_validation.hpp"
#include "datasim/vertical/validation/vertical_validation.hpp"
#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace datasim::vertical {

struct CircuitAssemblyIdTag {};
struct ComponentInstanceIdTag {};

using CircuitAssemblyId = component::StrongId<CircuitAssemblyIdTag>;
using ComponentInstanceId = component::StrongId<ComponentInstanceIdTag>;

enum class CircuitTopologyKind {
    series_rc_lowpass,
    unsupported_topology
};

enum class CircuitElementRole {
    series_resistor,
    shunt_capacitor,
    unspecified
};

// Preserves 1-based model terminal position, logical terminal name, physical package pin,
// and the circuit electrical node to which that pin is connected.
struct TerminalNodeBinding {
    std::size_t terminal_position = 0;
    std::string terminal_name;
    std::string physical_pin_id;
    std::string circuit_node_id;

    auto operator<=>(const TerminalNodeBinding&) const = default;
};

// Placed component instance referencing a single-component bundle, an explicit knowledge
// revision, an explicit model artifact version, and ordered terminal-to-node bindings.
struct CircuitComponentInstance {
    ComponentInstanceId instance_id;
    CircuitElementRole role = CircuitElementRole::unspecified;
    component::ComponentId component_id;
    component::ComponentKnowledgeId knowledge_id;
    std::uint32_t expected_knowledge_version = 1;
    component::ModelArtifactId selected_model_id;
    std::string selected_model_version;
    std::vector<TerminalNodeBinding> terminal_bindings;

    auto operator<=>(const CircuitComponentInstance&) const = default;
};

// Application-level circuit aggregate referencing multiple single-component
// ComponentDomainBundle records without altering ComponentDomainBundle's scope.
struct CircuitAssembly {
    CircuitAssemblyId id;
    std::string name;
    CircuitTopologyKind topology = CircuitTopologyKind::series_rc_lowpass;
    std::string input_node = "vin";
    std::string output_node = "vout";
    std::string ground_node = "0";
    std::vector<component::ComponentDomainBundle> component_bundles;
    std::vector<CircuitComponentInstance> instances;

    auto operator<=>(const CircuitAssembly&) const = default;
};

[[nodiscard]] const component::ComponentDomainBundle* find_bundle_for_component(
    const CircuitAssembly& assembly,
    const component::ComponentId& component_id) noexcept;

[[nodiscard]] const component::ModelArtifactDescriptor* find_model_in_bundle(
    const component::ComponentDomainBundle& bundle,
    const component::ModelArtifactId& model_id) noexcept;

[[nodiscard]] const CircuitComponentInstance* find_instance_by_role(
    const CircuitAssembly& assembly,
    CircuitElementRole role) noexcept;

[[nodiscard]] VerticalValidationReport validate_circuit_assembly(const CircuitAssembly& assembly);

} // namespace datasim::vertical

