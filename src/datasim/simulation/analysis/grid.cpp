#include "grid.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace datasim {
std::vector<double> validation_grid(const RcConfig& c, std::size_t intervals) {
    check_config(c);
    if (intervals == 0 || intervals > 1000000) throw std::invalid_argument("invalid grid size");
    const double start = c.delay_s + c.rise_s;
    std::vector<double> times;
    for (std::size_t i = 0; i <= intervals; ++i)
        times.push_back(i == intervals ? c.duration_s : start + (c.duration_s-start)*i/intervals);
    return times;
}
Samples interpolate(const Samples& samples, const std::vector<double>& times) {
    check_samples(samples);
    if (times.empty()) throw std::invalid_argument("empty interpolation grid");
    Samples result; double previous = -1;
    for (double t : times) {
        if (!std::isfinite(t) || t <= previous || t < samples.front().time_s || t > samples.back().time_s)
            throw std::invalid_argument("invalid interpolation grid or extrapolation requested");
        previous = t;
        auto right = std::lower_bound(samples.begin(), samples.end(), t,
            [](const Sample& s, double time) { return s.time_s < time; });
        if (right->time_s == t) { result.push_back(*right); continue; }
        const auto& left = *(right-1);
        const double fraction = (t-left.time_s)/(right->time_s-left.time_s);
        result.push_back({t, std::lerp(left.vin_V,right->vin_V,fraction), std::lerp(left.vout_V,right->vout_V,fraction)});
    }
    check_samples(result); return result;
}
double ramp_voltage(const RcConfig& c, double t) {
    const double tau = time_constant(c);
    if (!std::isfinite(t) || t < 0 || t > c.duration_s) throw std::invalid_argument("invalid ramp reference time");
    if (t <= c.delay_s) return c.initial_voltage_V;
    // Integral of the first-order impulse response under the PULSE linear rise.
    const double x = std::min(t-c.delay_s,c.rise_s)/tau;
    // x + expm1(-x) loses significant digits for the 1e-6 baseline x.
    const double integral = x < 1e-3 ? x*x*(0.5-x/6+x*x/24-x*x*x/120) : x+std::expm1(-x);
    const double during = c.voltage_V * tau/c.rise_s * integral;
    if (t <= c.delay_s+c.rise_s) return during;
    return c.voltage_V + (during-c.voltage_V)*std::exp(-(t-c.delay_s-c.rise_s)/tau);
}
}
