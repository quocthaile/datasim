#pragma once
#include "datasim/simulation/io/report.hpp"
#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include <compare>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace datasim::vertical {

struct ComponentProvenanceRecord {
    std::string instance_id;
    CircuitElementRole role = CircuitElementRole::unspecified;
    component::ComponentId component_id;
    std::string manufacturer;
    std::string part_number;
    std::optional<std::string> part_number_suffix;
    bool is_synthetic = false;
    component::ComponentKnowledgeId knowledge_id;
    std::uint32_t knowledge_version = 1;
    component::ReviewDecisionId knowledge_review_decision_id;
    std::string knowledge_reviewer_id;
    component::ModelArtifactId model_id;
    std::string model_version;
    component::ModelFormat model_format = component::ModelFormat::analytical_equation;
    component::ModelValidationStatus model_validation_status =
        component::ModelValidationStatus::unvalidated;
    std::vector<component::ModelValidationId> model_validation_record_ids;
    std::string model_parameter_name;
    component::NormalizedParameterId normalized_parameter_id;
    std::string canonical_parameter_name;
    component::ValueQualifier qualifier = component::ValueQualifier::typical;
    component::ParameterCategory category =
        component::ParameterCategory::electrical_characteristic;
    double resolved_si_value = 0.0;
    std::string normalized_si_unit;
    component::ObservationId source_observation_id;
    std::string preserved_original_value_text;
    std::string preserved_original_unit_symbol;
    std::string conversion_rule;
    std::vector<component::OperatingCondition> operating_conditions;
    std::vector<component::EvidenceId> evidence_location_ids;
    std::vector<component::SourceDocumentId> source_document_ids;
    std::vector<TerminalNodeBinding> terminal_bindings;

    auto operator<=>(const ComponentProvenanceRecord&) const = default;
};

// Pre-execution and post-execution provenance manifest for a vertical-slice RC run.
//
// Content fingerprint audit (FNV-1a-64, non-cryptographic):
// - configuration_content_id: datasim::content_id(datasim::config_json(rc_config)).
// - netlist_content_id: datasim::content_id(datasim::rc_netlist(rc_config)).
// - provenance_content_id: calculated BEFORE backend execution over the canonical JSON payload
//   containing circuit_id, execution_classification, is_synthetic_test_run, resistor
//   ComponentProvenanceRecord, capacitor ComponentProvenanceRecord, configuration_content_id,
//   and netlist_content_id. Post-execution runtime fields (backend_version, backend_executable,
//   execution_status, validation_status, error) and build/configure identity fields are recorded
//   in VerticalSliceProvenanceManifest and provenance_metadata.json, but are excluded from
//   provenance_content_id so identical pre-execution inputs yield identical content identities.
// - FNV-1a-64 is a fast deterministic content fingerprint for detecting accidental drift only;
//   it is NOT cryptographic integrity protection, a digital signature, tamper-proof provenance,
//   or authentication of source documents or reviewer identity.
struct VerticalSliceProvenanceManifest {
    std::uint32_t schema_version = 1;
    CircuitAssemblyId circuit_id;
    std::string circuit_name;
    ExecutionClassification execution_classification =
        ExecutionClassification::engineering_approved;
    bool is_synthetic_test_run = false;
    std::string parameter_origin = "component_knowledge_vertical_slice";
    ComponentProvenanceRecord resistor;
    ComponentProvenanceRecord capacitor;
    std::string configuration_json;
    std::string configuration_content_id;
    std::string netlist_content_id;
    std::string provenance_content_id;
    std::string backend_name = "ngspice";
    std::string backend_version;
    std::string backend_executable;
    std::string execution_status = "NOT_RUN";
    std::string validation_status = "NOT_RUN";
    std::string error;
    std::string application_version;
    std::string git_revision_at_configure;
    std::string source_sha256;

    auto operator<=>(const VerticalSliceProvenanceManifest&) const = default;
};

struct VerticalRcRunResult {
    datasim::RunResult run;
    AdaptedRcSimulation adapted;
    VerticalSliceProvenanceManifest provenance;
};

[[nodiscard]] VerticalSliceProvenanceManifest build_provenance_manifest(
    const CircuitAssembly& assembly,
    const AdaptedRcSimulation& adapted);

[[nodiscard]] std::string provenance_json(const VerticalSliceProvenanceManifest& manifest);

VerticalRcRunResult execute_vertical_rc_run(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs,
    const std::string& executable,
    const std::filesystem::path& new_directory);

} // namespace datasim::vertical

