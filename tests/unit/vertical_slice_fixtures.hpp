#pragma once
#include "datasim/vertical/adapter/rc_simulation_adapter.hpp"
#include "datasim/vertical/assembly/circuit_assembly.hpp"

namespace datasim::vertical::testing {

// Builds a valid synthetic series-RC CircuitAssembly from Phase 2 synthetic resistor
// and capacitor bundles with validated analytical_equation models (R = 1000 ohm, C = 1 uF).
// Also retains the Phase 2 capacitor spice_subcircuit model descriptor in the capacitor
// bundle so tests can verify deterministic rejection of external subcircuit models.
CircuitAssembly make_validated_synthetic_rc_assembly();

// Builds an equivalent non-synthetic engineering-reviewed series-RC CircuitAssembly
// for testing ExecutionPolicy::engineering_approved_only vs ExecutionPolicy::test_only_synthetic.
CircuitAssembly make_engineering_approved_rc_assembly();

// Default run inputs for synthetic test execution (ExecutionPolicy::test_only_synthetic).
RcRunInputSpecification make_synthetic_test_run_inputs();

} // namespace datasim::vertical::testing

