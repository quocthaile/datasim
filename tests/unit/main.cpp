#include "datasim/simulation/analysis/reference.hpp"
#include "datasim/simulation/backend/ngspice.hpp"
#include "datasim/simulation/validation/validation.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
using namespace datasim;
int checks = 0;
void require(bool condition) { ++checks; if (!condition) throw std::runtime_error("check failed #" + std::to_string(checks)); }
void rejects(const std::function<void()>& f) {
    bool threw = false; try { f(); } catch (const std::exception&) { threw = true; } require(threw);
}
int main() {
    try {
        RcConfig c;
        require(std::abs(time_constant(c) - 0.001) < 1e-15);
        require(analytical_voltage(c, 0) == 0);
        require(analytical_voltage(c, c.delay_s) == 0);
        require(std::abs(analytical_voltage(c, 0.002) - 3.1606027941427883) < 1e-12);
        require(std::abs(analytical_voltage(c, 0.006) - 4.966310265004573) < 1e-12);
        for (double bad : {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            auto x = c; x.resistance_ohm = bad; rejects([&]{ check_config(x); });
            x = c; x.capacitance_F = bad; rejects([&]{ check_config(x); });
            x = c; x.voltage_V = bad; rejects([&]{ check_config(x); });
        }
        for (auto mutate : std::vector<std::function<void(RcConfig&)>>{
            [](auto& x){ x.initial_voltage_V = 1; }, [](auto& x){ x.duration_s = 0.012; },
            [](auto& x){ x.step_s = 0.001; }, [](auto& x){ x.rise_s = 0.001; },
            [](auto& x){ x.period_s = 0.001; }, [](auto& x){ x.duration_s = 0.0005; }}) {
            auto x = c; mutate(x); rejects([&]{ rc_netlist(x); });
        }
        rejects([&]{ analytical_voltage(c, -1); });
        const auto net = rc_netlist(c);
        require(net.find("V1 vin 0 PULSE(0 5 ") != std::string::npos);
        require(net.find("R1 vin vout 1000") != std::string::npos);
        require(net.find(".ic V(vout)=0") != std::string::npos);
        require(net.find("wrdata samples.dat v(vin) v(vout)") != std::string::npos);
        auto other = c; other.resistance_ohm = 2000; other.voltage_V = 3;
        require(rc_netlist(other).find("R1 vin vout 2000") != std::string::npos);
        require(rc_netlist(other).find("PULSE(0 3 ") != std::string::npos);
        Samples samples;
        for (int i = 0; i <= 600; ++i) {
            const double t = c.duration_s * i / 600;
            samples.push_back({t, t <= c.delay_s ? 0. : 5., analytical_voltage(c, t)});
        }
        auto exact = validate(c, samples); require(exact.passed); require(exact.max_absolute_error_V == 0);
        for (auto& s : samples) if (s.time_s >= c.delay_s + c.rise_s) s.vout_V += 0.02;
        auto report = validate(c, samples);
        require(report.passed); require(std::abs(report.max_absolute_error_V - 0.02) < 1e-12);
        require(std::abs(report.rmse_V - 0.02) < 1e-12); require(std::abs(report.normalized_rmse - 0.004) < 1e-12);
        for (auto& s : samples) if (s.time_s >= c.delay_s + c.rise_s) s.vout_V += 0.05;
        require(!validate(c, samples).passed);
        auto short_run = samples; short_run.pop_back(); rejects([&]{ validate(c, short_run); });
        auto wrong_source = samples; wrong_source.back().vin_V = 0; rejects([&]{ validate(c, wrong_source); });
        std::ostringstream csv; write_csv(csv, samples); std::istringstream input(csv.str());
        const auto roundtrip = read_csv(input); require(roundtrip.size() == samples.size());
        require(roundtrip.back().vout_V == samples.back().vout_V);
        for (const auto* body : {"", "0,0,0\n0,0,0\n", "-1,0,0\n", "0,nan,0\n", "0,inf,0\n", "0,0\n", "0,0,0,2\n", "0,0,0junk\n", "0,,0 0\n", "0 0,0,\n", "\n"}) {
            std::istringstream in(std::string("time_s,vin_V,vout_V\n") + body); rejects([&]{ read_csv(in); });
        }
        std::istringstream bad_header("time,vin,vout\n0,0,0\n"); rejects([&]{ read_csv(bad_header); });
        std::istringstream table("time v(vin) v(vout)\n0 0 0\n0.001 5 1\n");
        require(read_ngspice_table(table).size() == 2);
        for (const auto* text : {"", "time v(vin) v(vout)\n", "time v(vout) v(vin)\n0 0 0\n", "time v(vin) v(vout)\n0 0 0 extra\n"}) {
            std::istringstream in(text); rejects([&]{ read_ngspice_table(in); });
        }
        Samples nonfinite{{0, 0, std::numeric_limits<double>::infinity()}};
        rejects([&]{ check_samples(nonfinite); });
        std::cout << "PASS: " << checks << " unit checks\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
