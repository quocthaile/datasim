#pragma once
#include "datasim/component/model/model_artifact.hpp"
#include "datasim/component/observation/operating_condition.hpp"
#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"
#include "datasim/vertical/compatibility/model_compatibility.hpp"
#include "datasim/vertical/resolution/parameter_resolution.hpp"
#include "datasim/vertical/validation/vertical_validation.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::vertical {

// Separate pulse voltage stimulus controls (independent of component knowledge).
struct RcPulseStimulus {
    double voltage_V = 5.0;
    double initial_voltage_V = 0.0;
    double delay_s = 1e-3;
    double rise_s = 1e-9;
    double fall_s = 1e-9;
    double width_s = 10e-3;
    double period_s = 20e-3;

    auto operator<=>(const RcPulseStimulus&) const = default;
};

// Separate transient timing controls (independent of component knowledge).
struct RcTransientControls {
    double step_s = 10e-6;
    double duration_s = 6e-3;

    auto operator<=>(const RcTransientControls&) const = default;
};

// Separate solver and operating-temperature controls (independent of component knowledge).
struct RcSolverControls {
    std::string integration_method = "trap";
    double reltol = 1e-6;
    double abstol_A = 1e-12;
    double vntol_V = 1e-9;
    double ambient_temperature_degC = 27.0;
    double nominal_temperature_degC = 27.0;

    auto operator<=>(const RcSolverControls&) const = default;
};

struct RcRunInputSpecification {
    component::AnalysisType requested_analysis = component::AnalysisType::transient;
    ExecutionPolicy execution_policy = ExecutionPolicy::engineering_approved_only;
    RcPulseStimulus stimulus;
    RcTransientControls transient;
    RcSolverControls solver;
    std::vector<component::OperatingCondition> additional_operating_conditions;

    auto operator<=>(const RcRunInputSpecification&) const = default;
};

struct AdaptedRcSimulation {
    datasim::RcConfig rc_config;
    RcRunInputSpecification run_inputs;
    ResolvedRcParameters resolved_parameters;
    ModelCompatibilityReport compatibility;
    ExecutionClassification classification = ExecutionClassification::engineering_approved;
};

struct RcAdapterPreparationResult {
    VerticalValidationReport validation;
    std::optional<AdaptedRcSimulation> adapted;
};

[[nodiscard]] datasim::RcConfig build_rc_config_from_resolved(
    const ResolvedRcParameters& resolved_parameters,
    const RcRunInputSpecification& run_inputs);

[[nodiscard]] RcAdapterPreparationResult prepare_vertical_rc_simulation(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs);

[[nodiscard]] AdaptedRcSimulation adapt_vertical_rc_simulation(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs);

} // namespace datasim::vertical
