#include "datasim/simulation/io/report.hpp"
#include "datasim/simulation/analysis/grid.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace datasim;
namespace fs=std::filesystem;
int main(int argc,char** argv) {
    try {
        if (argc!=3) throw std::invalid_argument("usage: rc_study NGSPICE NEW_OUTPUT_DIRECTORY");
        const auto root=fs::absolute(argv[2]); fs::create_directories(root.parent_path());
        if (!fs::create_directory(root)) throw std::runtime_error("study directory already exists");
        std::ofstream summary(root/"study.json"); summary.exceptions(std::ios::failbit|std::ios::badbit);
        summary << std::setprecision(17) << "{\"schema_version\":1,\"runs\":[\n";
        const RcConfig baseline; const auto grid=validation_grid(baseline);
        Samples original; bool all_passed=true; std::size_t index=0;
        for (double step : {10e-6,5e-6,2.5e-6,1.25e-6,10e-6,10e-6}) {
            RcConfig c; c.step_s=step;
            const auto name="run-"+std::to_string(index);
            const auto run=execute_rc_run(c,argv[1],root/name);
            const auto common=interpolate(run.samples,grid);
            long double ramp_squares=0; double ramp_max=0, interpolation_max=0;
            Samples exact_at_native;
            for (const auto& s : run.samples) exact_at_native.push_back({s.time_s,s.vin_V,ramp_voltage(c,s.time_s)});
            const auto ramp_interpolated=interpolate(exact_at_native,grid);
            for (std::size_t i=0;i<common.size();++i) {
                const double exact=ramp_voltage(c,grid[i]);
                const double error=common[i].vout_V-exact;
                ramp_squares+=static_cast<long double>(error)*error; ramp_max=std::max(ramp_max,std::abs(error));
                interpolation_max=std::max(interpolation_max,std::abs(ramp_interpolated[i].vout_V-exact));
            }
            double repeat_delta=0;
            if (index==0) original=common;
            if (index>=4) for (std::size_t i=0;i<common.size();++i)
                repeat_delta=std::max({repeat_delta,std::abs(common[i].vout_V-original[i].vout_V),std::abs(common[i].vin_V-original[i].vin_V)});
            const double repeat_tolerance=1e-12;
            all_passed=all_passed && run.validation.passed && repeat_delta<=repeat_tolerance;
            if (index) summary << ",\n";
            summary << "{\"directory\":" << json_string(name) << ",\"maximum_step_s\":" << step
                << ",\"backend_seconds\":" << run.backend_seconds << ",\"sample_count\":" << run.samples.size()
                << ",\"grid_count\":" << common.size() << ",\"maximum_absolute_error_V\":" << run.validation.max_absolute_error_V
                << ",\"rmse_V\":" << run.validation.rmse_V << ",\"nrmse\":" << run.validation.normalized_rmse
                << ",\"ramp_reference_max_error_V\":" << ramp_max << ",\"ramp_reference_rmse_V\":" << std::sqrt(ramp_squares/common.size())
                << ",\"reference_interpolation_max_error_V\":" << interpolation_max
                << ",\"repeat_comparison\":" << (index>=4?"true":"false") << ",\"repeat_max_delta_V\":" << repeat_delta
                << ",\"repeat_tolerance_V\":" << repeat_tolerance << '}';
            std::cout << name << " step=" << step << " max_error_V=" << run.validation.max_absolute_error_V << " RMSE=" << run.validation.rmse_V << '\n';
            ++index;
        }
        summary << "\n],\"all_acceptance_checks_passed\":" << (all_passed?"true":"false") << "}\n"; summary.close();
        return all_passed?0:1;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
