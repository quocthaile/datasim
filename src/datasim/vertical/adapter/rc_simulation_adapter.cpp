#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include <cmath>
#include <exception>

namespace datasim::vertical {

namespace {

void validate_solver_controls(
    VerticalValidationReport& report,
    const RcSolverControls& solver) {
    if (!std::isfinite(solver.ambient_temperature_degC) ||
        !std::isfinite(solver.nominal_temperature_degC)) {
        report.add(
            VerticalValidationCode::invalid_operating_condition,
            "run_inputs.solver.temperature",
            "solver ambient and nominal temperatures must be finite");
        return;
    }
    if (solver.integration_method != "trap" ||
        solver.reltol != 1e-6 ||
        solver.abstol_A != 1e-12 ||
        solver.vntol_V != 1e-9 ||
        solver.ambient_temperature_degC != 27.0 ||
        solver.nominal_temperature_degC != 27.0) {
        report.add(
            VerticalValidationCode::unsupported_solver_controls,
            "run_inputs.solver",
            "Phase 1/1.5 ideal-RC backend requires method='trap', reltol=1e-6, abstol=1e-12, vntol=1e-9, temp=27 degC, tnom=27 degC");
    }
}

} // namespace

datasim::RcConfig build_rc_config_from_resolved(
    const ResolvedRcParameters& resolved_parameters,
    const RcRunInputSpecification& run_inputs) {
    datasim::RcConfig c;
    c.resistance_ohm = resolved_parameters.resistor.resolved_si_value;
    c.capacitance_F = resolved_parameters.capacitor.resolved_si_value;
    c.voltage_V = run_inputs.stimulus.voltage_V;
    c.initial_voltage_V = run_inputs.stimulus.initial_voltage_V;
    c.delay_s = run_inputs.stimulus.delay_s;
    c.rise_s = run_inputs.stimulus.rise_s;
    c.fall_s = run_inputs.stimulus.fall_s;
    c.width_s = run_inputs.stimulus.width_s;
    c.period_s = run_inputs.stimulus.period_s;
    c.step_s = run_inputs.transient.step_s;
    c.duration_s = run_inputs.transient.duration_s;
    datasim::check_config(c);
    return c;
}

RcAdapterPreparationResult prepare_vertical_rc_simulation(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs) {
    RcAdapterPreparationResult out;

    const auto resolution = resolve_rc_parameters(assembly);
    out.validation.append(resolution.validation);
    if (!resolution.parameters.has_value()) {
        return out;
    }

    OperatingEnvironment env;
    env.ambient_temperature_degC = run_inputs.solver.ambient_temperature_degC;
    env.stimulus_voltage_V = run_inputs.stimulus.voltage_V;
    env.initial_voltage_V = run_inputs.stimulus.initial_voltage_V;
    env.additional_conditions = run_inputs.additional_operating_conditions;

    const auto compat = evaluate_rc_model_compatibility(
        assembly,
        *resolution.parameters,
        run_inputs.requested_analysis,
        env,
        run_inputs.execution_policy);
    out.validation.append(compat.validation);

    validate_solver_controls(out.validation, run_inputs.solver);

    datasim::RcConfig cfg{};
    try {
        cfg = build_rc_config_from_resolved(*resolution.parameters, run_inputs);
    } catch (const std::exception& e) {
        out.validation.add(
            VerticalValidationCode::invalid_rc_simulation_configuration,
            "rc_config",
            e.what());
    }

    if (out.validation.valid()) {
        AdaptedRcSimulation adapted;
        adapted.rc_config = cfg;
        adapted.run_inputs = run_inputs;
        adapted.resolved_parameters = *resolution.parameters;
        adapted.compatibility = compat;
        adapted.classification = compat.classification;
        out.adapted = std::move(adapted);
    }

    return out;
}

AdaptedRcSimulation adapt_vertical_rc_simulation(
    const CircuitAssembly& assembly,
    const RcRunInputSpecification& run_inputs) {
    auto prep = prepare_vertical_rc_simulation(assembly, run_inputs);
    ensure_vertical_valid(prep.validation);
    return std::move(*prep.adapted);
}

} // namespace datasim::vertical
