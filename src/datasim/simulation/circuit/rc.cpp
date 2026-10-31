#include "rc.hpp"
#include <cmath>
#include <stdexcept>
#include <string>
namespace datasim {
void check_config(const RcConfig& c) {
    auto positive = [](double v, const char* name) {
        if (!std::isfinite(v) || v <= 0)
            throw std::invalid_argument(std::string(name) + " must be finite and positive");
    };
    positive(c.resistance_ohm, "resistance"); positive(c.capacitance_F, "capacitance");
    positive(c.voltage_V, "step voltage"); positive(c.delay_s, "delay");
    positive(c.rise_s, "rise time"); positive(c.fall_s, "fall time");
    positive(c.width_s, "pulse width"); positive(c.period_s, "period");
    positive(c.step_s, "time step"); positive(c.duration_s, "duration");
    const double tau = c.resistance_ohm * c.capacitance_F;
    positive(tau, "RC time constant");
    positive(c.delay_s + c.rise_s + c.width_s + c.fall_s, "pulse timing sum");
    if (c.initial_voltage_V != 0 || c.step_s > tau / 10 || c.rise_s > tau * 1e-3 ||
        c.delay_s + c.rise_s + c.step_s >= c.duration_s ||
        c.duration_s >= c.delay_s + c.rise_s + c.width_s ||
        c.period_s <= c.rise_s + c.width_s + c.fall_s ||
        c.duration_s / c.step_s > 1e6)
        throw std::invalid_argument("unsupported RC configuration: require zero IC, small rise/step, charging-only duration and <=1e6 nominal steps");
}
double time_constant(const RcConfig& c) { check_config(c); return c.resistance_ohm * c.capacitance_F; }
}
