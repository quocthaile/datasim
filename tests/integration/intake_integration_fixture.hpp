#pragma once

#include "datasim/vertical/assembly/circuit_assembly.hpp"

namespace datasim::vertical::testing {

// Builds the RC assembly from synthetic intake candidates and the existing
// reviewed synthetic model descriptors.
CircuitAssembly make_intake_derived_rc_assembly();

} // namespace datasim::vertical::testing
