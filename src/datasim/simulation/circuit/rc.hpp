#pragma once
namespace datasim {
// Manually configured ideal components, not datasheet-extracted observations.
struct RcConfig {
    double resistance_ohm = 1000;
    double capacitance_F = 1e-6;
    double voltage_V = 5;
    double initial_voltage_V = 0;
    double delay_s = 1e-3;
    double rise_s = 1e-9;
    double fall_s = 1e-9;
    double width_s = 10e-3;
    double period_s = 20e-3;
    double step_s = 10e-6;
    double duration_s = 6e-3;
};
void check_config(const RcConfig&);
double time_constant(const RcConfig&);
}
