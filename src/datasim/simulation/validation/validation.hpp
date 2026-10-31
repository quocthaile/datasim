#pragma once
#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/simulation/io/samples.hpp"
namespace datasim {
struct ErrorPoint { double time_s, reference_V, error_V, absolute_error_V; };
struct Validation {
    std::vector<ErrorPoint> errors;
    double max_absolute_error_V = 0, rmse_V = 0, normalized_rmse = 0, tolerance_V = 0;
    bool passed = false;
};
Validation validate(const RcConfig&, const Samples&);
Validation validate_on_grid(const RcConfig&, const Samples&, const std::vector<double>&);
void write_errors(std::ostream&, const Validation&);
}
