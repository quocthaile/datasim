#include "reference.hpp"
#include <cmath>
#include <stdexcept>
namespace datasim {
double analytical_voltage(const RcConfig& c, double t) {
    const double tau = time_constant(c);
    if (!std::isfinite(t) || t < 0 || t > c.duration_s)
        throw std::invalid_argument("reference time outside simulation interval");
    return t <= c.delay_s ? 0 : -c.voltage_V * std::expm1(-(t - c.delay_s) / tau);
}
}
