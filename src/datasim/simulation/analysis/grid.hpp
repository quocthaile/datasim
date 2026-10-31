#pragma once
#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/simulation/io/samples.hpp"
namespace datasim {
// Includes both endpoints; independent of the solver's adaptive timestep.
std::vector<double> validation_grid(const RcConfig&, std::size_t intervals = 4000);
Samples interpolate(const Samples&, const std::vector<double>& times);
// Diagnostic reference for the actual linear ramp, not a relaxed acceptance test.
double ramp_voltage(const RcConfig&, double time_s);
}
