#pragma once
#include "datasim/component/identity/ids.hpp"
#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

enum class ModelFormat {
    analytical_equation,
    spice_model_card,
    spice_subcircuit,
    lookup_table,
    vendor_specific_artifact
};

enum class AnalysisType {
    dc_operating_point,
    dc_sweep,
    ac_small_signal,
    transient,
    noise,
    thermal
};

// Explicit 1-based ordered mapping between a logical model terminal and a package pin.
struct TerminalDescriptor {
    std::size_t position = 0;
    std::string terminal_name;
    std::string physical_pin_id;
    std::optional<std::string> description;

    auto operator<=>(const TerminalDescriptor&) const = default;
};

struct ModelParameterMapping {
    std::string model_parameter_name;
    std::optional<NormalizedParameterId> knowledge_param_id;
    std::optional<double> fixed_value;
    std::string expected_unit;
    std::optional<std::string> mapping_expression;

    auto operator<=>(const ModelParameterMapping&) const = default;
};

struct NumericRangeConstraint {
    std::string quantity_name;
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::string unit_symbol;
    bool min_inclusive = true;
    bool max_inclusive = true;

    auto operator<=>(const NumericRangeConstraint&) const = default;
};

struct ModelValidityEnvelope {
    std::vector<NumericRangeConstraint> operating_ranges;
    std::vector<std::string> assumptions;
    std::optional<std::string> notes;

    auto operator<=>(const ModelValidityEnvelope&) const = default;
};

struct ModelLicenseMetadata {
    std::string license_identifier;
    bool redistribution_permitted = false;
    std::string usage_restrictions;

    auto operator<=>(const ModelLicenseMetadata&) const = default;
};

// Metadata contract describing a mathematical or vendor model artifact.
// Valid registration metadata never implies numerical accuracy or model validation.
struct ModelArtifactDescriptor {
    ModelArtifactId id;
    ComponentId component_id;
    std::optional<ComponentKnowledgeId> component_knowledge_id;
    std::string version;
    ModelFormat format = ModelFormat::analytical_equation;
    std::string source_vendor;
    std::optional<std::string> artifact_uri;
    std::optional<std::string> artifact_sha256;
    ModelLicenseMetadata license;
    std::vector<TerminalDescriptor> terminals;
    std::vector<ModelParameterMapping> parameter_mappings;
    ModelValidityEnvelope validity_envelope;
    std::vector<AnalysisType> supported_analyses;
    std::vector<std::string> limitations;
    std::vector<ModelValidationId> validation_record_refs;
    bool is_synthetic = false;

    auto operator<=>(const ModelArtifactDescriptor&) const = default;
};

} // namespace datasim::component
