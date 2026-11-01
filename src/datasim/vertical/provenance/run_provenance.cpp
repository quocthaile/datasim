#include "datasim/vertical/provenance/run_provenance.hpp"
#include "datasim/build_info.hpp"
#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/backend/process.hpp"
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace datasim::vertical {

namespace {

namespace fs = std::filesystem;

std::ostringstream classic_stream() {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    return out;
}

const char* classification_str(ExecutionClassification c) noexcept {
    switch (c) {
        case ExecutionClassification::engineering_approved:
            return "engineering_approved";
        case ExecutionClassification::test_only_synthetic:
            return "test_only_synthetic";
    }
    return "engineering_approved";
}

const char* role_str(CircuitElementRole r) noexcept {
    switch (r) {
        case CircuitElementRole::series_resistor:
            return "series_resistor";
        case CircuitElementRole::shunt_capacitor:
            return "shunt_capacitor";
        case CircuitElementRole::unspecified:
            return "unspecified";
    }
    return "unspecified";
}

const char* model_format_str(component::ModelFormat f) noexcept {
    switch (f) {
        case component::ModelFormat::analytical_equation:
            return "analytical_equation";
        case component::ModelFormat::spice_model_card:
            return "spice_model_card";
        case component::ModelFormat::spice_subcircuit:
            return "spice_subcircuit";
        case component::ModelFormat::lookup_table:
            return "lookup_table";
        case component::ModelFormat::vendor_specific_artifact:
            return "vendor_specific_artifact";
    }
    return "analytical_equation";
}

const char* model_validation_status_str(component::ModelValidationStatus s) noexcept {
    switch (s) {
        case component::ModelValidationStatus::unvalidated:
            return "unvalidated";
        case component::ModelValidationStatus::validated_within_envelope:
            return "validated_within_envelope";
        case component::ModelValidationStatus::failed_validation:
            return "failed_validation";
        case component::ModelValidationStatus::out_of_date:
            return "out_of_date";
    }
    return "unvalidated";
}

const char* qualifier_str(component::ValueQualifier q) noexcept {
    switch (q) {
        case component::ValueQualifier::minimum:
            return "minimum";
        case component::ValueQualifier::typical:
            return "typical";
        case component::ValueQualifier::maximum:
            return "maximum";
        case component::ValueQualifier::unspecified:
            return "unspecified";
    }
    return "unspecified";
}

const char* category_str(component::ParameterCategory c) noexcept {
    switch (c) {
        case component::ParameterCategory::absolute_maximum_rating:
            return "absolute_maximum_rating";
        case component::ParameterCategory::recommended_operating_condition:
            return "recommended_operating_condition";
        case component::ParameterCategory::electrical_characteristic:
            return "electrical_characteristic";
        case component::ParameterCategory::thermal_characteristic:
            return "thermal_characteristic";
        case component::ParameterCategory::other_unspecified:
            return "other_unspecified";
    }
    return "other_unspecified";
}

ComponentProvenanceRecord build_component_provenance(
    const CircuitAssembly& assembly,
    const ResolvedComponentParameter& resolved,
    component::ModelValidationStatus val_status,
    const std::vector<component::ModelValidationId>& val_ids) {
    ComponentProvenanceRecord rec;
    rec.instance_id = resolved.instance_id.value;
    rec.role = resolved.role;
    rec.component_id = resolved.component.id;
    rec.manufacturer = resolved.component.manufacturer;
    rec.part_number = resolved.component.part_number;
    rec.part_number_suffix = resolved.component.part_number_suffix;
    rec.is_synthetic = resolved.component.is_synthetic;
    rec.knowledge_id = resolved.knowledge_id;
    rec.knowledge_version = resolved.knowledge_version;

    const auto* bundle = find_bundle_for_component(assembly, resolved.component.id);
    if (bundle != nullptr && bundle->knowledge.has_value()) {
        rec.knowledge_review_decision_id = bundle->knowledge->review_decision.id;
        rec.knowledge_reviewer_id = bundle->knowledge->review_decision.reviewer.id;
    }

    rec.model_id = resolved.model_id;
    rec.model_version = resolved.model_version;
    if (bundle != nullptr) {
        const auto* model = find_model_in_bundle(*bundle, resolved.model_id);
        if (model != nullptr) {
            rec.model_format = model->format;
        }
    }
    rec.model_validation_status = val_status;
    rec.model_validation_record_ids = val_ids;
    rec.model_parameter_name = resolved.model_parameter_name;
    rec.normalized_parameter_id = resolved.normalized_parameter_id;
    rec.canonical_parameter_name = resolved.canonical_name;
    rec.qualifier = resolved.qualifier;
    rec.category = resolved.category;
    rec.resolved_si_value = resolved.resolved_si_value;
    rec.normalized_si_unit = resolved.normalized_si_unit;
    rec.source_observation_id = resolved.source_observation_id;
    rec.preserved_original_value_text = resolved.preserved_original_value_text;
    rec.preserved_original_unit_symbol = resolved.preserved_original_unit_symbol;
    rec.conversion_rule = resolved.conversion_rule;
    rec.operating_conditions = resolved.operating_conditions;

    for (const auto& ev : resolved.source_evidence_locations) {
        rec.evidence_location_ids.push_back(ev.id);
    }
    for (const auto& doc : resolved.source_documents) {
        rec.source_document_ids.push_back(doc.id);
    }
    rec.terminal_bindings = resolved.terminal_bindings;
    return rec;
}

std::string component_provenance_json(const ComponentProvenanceRecord& r) {
    auto out = classic_stream();
    out << "{\"instance_id\":" << datasim::json_string(r.instance_id)
        << ",\"role\":" << datasim::json_string(role_str(r.role))
        << ",\"component_id\":" << datasim::json_string(r.component_id.value)
        << ",\"manufacturer\":" << datasim::json_string(r.manufacturer)
        << ",\"part_number\":" << datasim::json_string(r.part_number)
        << ",\"part_number_suffix\":"
        << (r.part_number_suffix.has_value()
                ? datasim::json_string(*r.part_number_suffix)
                : std::string("null"))
        << ",\"is_synthetic\":" << (r.is_synthetic ? "true" : "false")
        << ",\"knowledge_id\":" << datasim::json_string(r.knowledge_id.value)
        << ",\"knowledge_version\":" << r.knowledge_version
        << ",\"knowledge_review_decision_id\":"
        << datasim::json_string(r.knowledge_review_decision_id.value)
        << ",\"knowledge_reviewer_id\":" << datasim::json_string(r.knowledge_reviewer_id)
        << ",\"model_id\":" << datasim::json_string(r.model_id.value)
        << ",\"model_version\":" << datasim::json_string(r.model_version)
        << ",\"model_format\":" << datasim::json_string(model_format_str(r.model_format))
        << ",\"model_validation_status\":"
        << datasim::json_string(model_validation_status_str(r.model_validation_status))
        << ",\"model_validation_record_ids\":[";
    for (std::size_t i = 0; i < r.model_validation_record_ids.size(); ++i) {
        if (i) {
            out << ',';
        }
        out << datasim::json_string(r.model_validation_record_ids[i].value);
    }
    out << "],\"model_parameter_name\":" << datasim::json_string(r.model_parameter_name)
        << ",\"normalized_parameter_id\":"
        << datasim::json_string(r.normalized_parameter_id.value)
        << ",\"canonical_parameter_name\":"
        << datasim::json_string(r.canonical_parameter_name)
        << ",\"qualifier\":" << datasim::json_string(qualifier_str(r.qualifier))
        << ",\"category\":" << datasim::json_string(category_str(r.category))
        << ",\"resolved_si_value\":" << r.resolved_si_value
        << ",\"normalized_si_unit\":" << datasim::json_string(r.normalized_si_unit)
        << ",\"source_observation_id\":" << datasim::json_string(r.source_observation_id.value)
        << ",\"preserved_original_value_text\":"
        << datasim::json_string(r.preserved_original_value_text)
        << ",\"preserved_original_unit_symbol\":"
        << datasim::json_string(r.preserved_original_unit_symbol)
        << ",\"conversion_rule\":" << datasim::json_string(r.conversion_rule)
        << ",\"operating_conditions\":[";
    for (std::size_t i = 0; i < r.operating_conditions.size(); ++i) {
        if (i) {
            out << ',';
        }
        const auto& c = r.operating_conditions[i];
        out << "{\"parameter_name\":" << datasim::json_string(c.parameter_name)
            << ",\"original_value_text\":" << datasim::json_string(c.original_value_text)
            << ",\"unit_symbol\":" << datasim::json_string(c.unit_symbol)
            << ",\"numeric_value\":";
        if (c.numeric_value.has_value()) {
            out << *c.numeric_value;
        } else {
            out << "null";
        }
        out << '}';
    }
    out << "],\"evidence_location_ids\":[";
    for (std::size_t i = 0; i < r.evidence_location_ids.size(); ++i) {
        if (i) {
            out << ',';
        }
        out << datasim::json_string(r.evidence_location_ids[i].value);
    }
    out << "],\"source_document_ids\":[";
    for (std::size_t i = 0; i < r.source_document_ids.size(); ++i) {
        if (i) {
            out << ',';
        }
        out << datasim::json_string(r.source_document_ids[i].value);
    }
    out << "],\"terminal_bindings\":[";
    for (std::size_t i = 0; i < r.terminal_bindings.size(); ++i) {
        if (i) {
            out << ',';
        }
        const auto& tb = r.terminal_bindings[i];
        out << "{\"terminal_position\":" << tb.terminal_position
            << ",\"terminal_name\":" << datasim::json_string(tb.terminal_name)
            << ",\"physical_pin_id\":" << datasim::json_string(tb.physical_pin_id)
            << ",\"circuit_node_id\":" << datasim::json_string(tb.circuit_node_id)
            << '}';
    }
    out << "]}";
    return out.str();
}

std::string canonical_provenance_payload(const VerticalSliceProvenanceManifest& m) {
    auto out = classic_stream();
    out << "{\"circuit_id\":" << datasim::json_string(m.circuit_id.value)
        << ",\"execution_classification\":"
        << datasim::json_string(classification_str(m.execution_classification))
        << ",\"is_synthetic_test_run\":" << (m.is_synthetic_test_run ? "true" : "false")
        << ",\"resistor\":" << component_provenance_json(m.resistor)
        << ",\"capacitor\":" << component_provenance_json(m.capacitor)
        << ",\"configuration_content_id\":" << datasim::json_string(m.configuration_content_id)
        << ",\"netlist_content_id\":" << datasim::json_string(m.netlist_content_id) << '}';
    return out.str();
}

void write_file_binary(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out << text;
    out.close();
}

std::string read_optional_file(const fs::path& path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

VerticalSliceProvenanceManifest build_provenance_manifest(
    const CircuitAssembly& assembly,
    const AdaptedRcSimulation& adapted) {
    VerticalSliceProvenanceManifest m;
    m.schema_version = 1U;
    m.circuit_id = assembly.id;
    m.circuit_name = assembly.name;
    m.execution_classification = adapted.classification;
    m.is_synthetic_test_run =
        (adapted.classification == ExecutionClassification::test_only_synthetic);
    m.parameter_origin = "component_knowledge_vertical_slice";

    m.resistor = build_component_provenance(
        assembly,
        adapted.resolved_parameters.resistor,
        adapted.compatibility.resistor_validation_status,
        adapted.compatibility.resistor_validation_record_ids);

    m.capacitor = build_component_provenance(
        assembly,
        adapted.resolved_parameters.capacitor,
        adapted.compatibility.capacitor_validation_status,
        adapted.compatibility.capacitor_validation_record_ids);

    m.configuration_json = datasim::config_json(adapted.rc_config);
    m.configuration_content_id = datasim::content_id(m.configuration_json);
    m.netlist_content_id = datasim::content_id(datasim::rc_netlist(adapted.rc_config));
    m.backend_name = "ngspice";
    m.execution_status = "NOT_RUN";
    m.validation_status = "NOT_RUN";
    m.application_version = DATASIM_VERSION;
    m.git_revision_at_configure = DATASIM_GIT_REVISION;
    m.source_sha256 = DATASIM_SOURCE_SHA256;
    m.provenance_content_id = datasim::content_id(canonical_provenance_payload(m));
    return m;
}

std::string provenance_json(const VerticalSliceProvenanceManifest& m) {
    auto out = classic_stream();
    out << "{\n\"schema_version\":" << m.schema_version
        << ",\"circuit_id\":" << datasim::json_string(m.circuit_id.value)
        << ",\"circuit_name\":" << datasim::json_string(m.circuit_name)
        << ",\n\"execution_classification\":"
        << datasim::json_string(classification_str(m.execution_classification))
        << ",\"is_synthetic_test_run\":" << (m.is_synthetic_test_run ? "true" : "false")
        << ",\"parameter_origin\":" << datasim::json_string(m.parameter_origin)
        << ",\n\"resistor\":" << component_provenance_json(m.resistor)
        << ",\n\"capacitor\":" << component_provenance_json(m.capacitor)
        << ",\n\"configuration\":" << m.configuration_json
        << ",\n\"configuration_hash_algorithm\":\"FNV-1a-64 (non-cryptographic)\""
        << ",\"configuration_content_id\":" << datasim::json_string(m.configuration_content_id)
        << ",\"netlist_hash_algorithm\":\"FNV-1a-64 (non-cryptographic)\""
        << ",\"netlist_content_id\":" << datasim::json_string(m.netlist_content_id)
        << ",\"provenance_hash_algorithm\":\"FNV-1a-64 (non-cryptographic)\""
        << ",\"provenance_content_id\":" << datasim::json_string(m.provenance_content_id)
        << ",\n\"backend\":" << datasim::json_string(m.backend_name)
        << ",\"backend_version\":" << datasim::json_string(m.backend_version)
        << ",\"backend_executable\":" << datasim::json_string(m.backend_executable)
        << ",\"execution_status\":" << datasim::json_string(m.execution_status)
        << ",\"validation_status\":" << datasim::json_string(m.validation_status)
        << ",\"error\":" << datasim::json_string(m.error)
        << ",\n\"application_version\":" << datasim::json_string(m.application_version)
        << ",\"git_revision_at_configure\":"
        << datasim::json_string(m.git_revision_at_configure)
        << ",\"source_sha256\":" << datasim::json_string(m.source_sha256)
        << "\n}\n";
    return out.str();
}

VerticalRcRunResult execute_vertical_rc_run(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs,
    const std::string& executable,
    const fs::path& new_directory) {
    // Validate circuit assembly, parameter resolution, model compatibility, and RcConfig
    // before touching the filesystem or launching a process.
    AdaptedRcSimulation adapted = adapt_vertical_rc_simulation(assembly, run_inputs);
    VerticalSliceProvenanceManifest manifest = build_provenance_manifest(assembly, adapted);

    const auto dir = fs::absolute(new_directory);
    const auto found = datasim::find_executable(executable);
    manifest.backend_executable = found.string();

    try {
        auto run = datasim::execute_rc_run(adapted.rc_config, executable, dir);
        manifest.backend_version = read_optional_file(dir / "version.txt");
        manifest.execution_status = "SUCCEEDED";
        manifest.validation_status = "PASSED";
        write_file_binary(dir / "provenance_metadata.json", provenance_json(manifest));
        return VerticalRcRunResult{std::move(run), std::move(adapted), std::move(manifest)};
    } catch (const std::exception& e) {
        manifest.error = e.what();
        manifest.backend_version = read_optional_file(dir / "version.txt");
        if (fs::exists(dir / "results.csv")) {
            manifest.execution_status = "SUCCEEDED";
            manifest.validation_status = "FAILED";
        } else if (fs::exists(dir / "version.txt")) {
            manifest.execution_status = "FAILED";
        }
        if (fs::exists(dir) && fs::is_directory(dir)) {
            write_file_binary(dir / "provenance_metadata.json", provenance_json(manifest));
        }
        throw;
    }
}

} // namespace datasim::vertical

