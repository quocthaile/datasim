#include "validation.hpp"
#include "datasim/simulation/analysis/reference.hpp"
#include "datasim/simulation/analysis/grid.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <stdexcept>
namespace datasim {
Validation validate(const RcConfig& c, const Samples& s) {
    check_config(c); check_samples(s);
    const double epsilon = std::max(1e-15, c.duration_s * 1e-10);
    if (s.size() < 3 || s.front().time_s != 0 ||
        std::abs(s.back().time_s - c.duration_s) > epsilon)
        throw std::runtime_error("incomplete simulation time coverage");
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i].time_s > c.duration_s + epsilon ||
            (i && s[i].time_s - s[i-1].time_s > c.step_s * 1.01))
            throw std::runtime_error("incomplete simulation sampling");
        const double t = s[i].time_s;
        const double vin = t <= c.delay_s ? 0 : c.voltage_V * std::min(1.0, (t - c.delay_s) / c.rise_s);
        if (std::abs(s[i].vin_V - vin) > c.voltage_V * 1e-5)
            throw std::runtime_error("input source does not match configured pulse");
        if (t <= c.delay_s && std::abs(s[i].vout_V) > c.voltage_V * 0.01)
            throw std::runtime_error("nonzero pre-step response");
    }
    Validation v; v.tolerance_V = 0.01 * c.voltage_V;
    long double squares = 0;
    for (const auto& x : s) {
        if (x.time_s < c.delay_s + c.rise_s) continue;
        const double reference = analytical_voltage(c, std::min(x.time_s, c.duration_s));
        const double error = x.vout_V - reference;
        if (!std::isfinite(error)) throw std::runtime_error("non-finite validation error");
        v.errors.push_back({x.time_s, reference, error, std::abs(error)});
        v.max_absolute_error_V = std::max(v.max_absolute_error_V, std::abs(error));
        squares += static_cast<long double>(error) * error;
    }
    if (v.errors.empty()) throw std::runtime_error("no post-transition samples");
    v.rmse_V = static_cast<double>(std::sqrt(squares / v.errors.size()));
    v.normalized_rmse = v.rmse_V / c.voltage_V;
    v.passed = v.max_absolute_error_V <= v.tolerance_V;
    return v;
}
Validation validate_on_grid(const RcConfig& c, const Samples& samples, const std::vector<double>& times) {
    // Preserve native-grid integrity AND acceptance; resampling cannot hide a spike.
    const auto native = validate(c, samples);
    if (times.empty() || times.front() != c.delay_s+c.rise_s || times.back() != c.duration_s)
        throw std::invalid_argument("validation grid must span the entire post-rise interval");
    const auto common = interpolate(samples, times);
    Validation v; v.tolerance_V = 0.01*c.voltage_V;
    long double squares = 0;
    for (const auto& sample : common) {
        const double reference = analytical_voltage(c,sample.time_s);
        const double error = sample.vout_V-reference;
        v.errors.push_back({sample.time_s,reference,error,std::abs(error)});
        v.max_absolute_error_V = std::max(v.max_absolute_error_V,std::abs(error));
        squares += static_cast<long double>(error)*error;
    }
    v.rmse_V = static_cast<double>(std::sqrt(squares/v.errors.size()));
    v.normalized_rmse = v.rmse_V/c.voltage_V;
    v.passed = native.passed && v.max_absolute_error_V <= v.tolerance_V;
    return v;
}
void write_errors(std::ostream& out, const Validation& v) {
    out.imbue(std::locale::classic()); out << std::setprecision(17);
    out << "time_s,reference_V,error_V,absolute_error_V\n";
    for (const auto& e : v.errors) out << e.time_s << ',' << e.reference_V << ',' << e.error_V << ',' << e.absolute_error_V << '\n';
    if (!out) throw std::runtime_error("error CSV write failure");
}
}
