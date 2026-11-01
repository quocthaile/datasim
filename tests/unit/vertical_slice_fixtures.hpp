#pragma once
#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"

namespace datasim::vertical::testing {

// Builds a valid synthetic series-RC CircuitAssembly from Phase 2 synthetic resistor
// and capacitor bundles with validated analytical_equation models (R = 1000 ohm, C = 1 uF).
// Also retains the Phase 2 capacitor spice_subcircuit model descriptor in the capacitor
// bundle so tests can verify deterministic rejection of external subcircuit models.
CircuitAssembly make_validated_synthetic_rc_assembly();

// Builds a metadata-classified non-synthetic policy-gate test fixture derived from the
// synthetic RC assembly by setting non-synthetic metadata flags (is_synthetic = false,
// ExtractionMethod::manual_entry). Used solely to test the ExecutionPolicy::engineering_approved_only
// vs ExecutionPolicy::test_only_synthetic policy gate; does NOT represent real manufacturer
// datasheet extraction, authenticated reviewer identity, or physical device qualification.
CircuitAssembly make_non_synthetic_policy_gate_rc_assembly();

// Returns an OperatingEnvironment with the default 27 degC ambient temperature, 5 V pulse
// stimulus, 0 V initial voltage, and an explicit documented signal_frequency = 50 Hz
// (fundamental pulse repetition rate 1 / 20 ms) required by the synthetic RC model validity envelopes.
OperatingEnvironment make_documented_rc_operating_environment();

// Default run inputs for synthetic test execution (ExecutionPolicy::test_only_synthetic)
// including the explicit documented signal_frequency = 50 Hz operating condition.
RcRunInputSpecification make_synthetic_test_run_inputs();

} // namespace datasim::vertical::testing
