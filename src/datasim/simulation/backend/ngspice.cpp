#include "ngspice.hpp"
#include "process.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace datasim {
namespace fs = std::filesystem;
std::string rc_netlist(const RcConfig& c) {
    check_config(c);
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17);
    out << "Datasim manually configured ideal RC transient\n"
        << "V1 vin 0 PULSE(0 " << c.voltage_V << ' ' << c.delay_s << ' ' << c.rise_s << ' '
        << c.fall_s << ' ' << c.width_s << ' ' << c.period_s << ")\n"
        << "R1 vin vout " << c.resistance_ohm << "\nC1 vout 0 " << c.capacitance_F << " IC=0\n"
        << ".ic V(vout)=0\n.options reltol=1e-6 abstol=1e-12 vntol=1e-9 method=trap temp=27 tnom=27\n.control\n"
        << "set noaskquit\nset wr_singlescale\nset wr_vecnames\nset numdgt=16\n"
        << "tran " << c.step_s << ' ' << c.duration_s << " 0 " << c.step_s << '\n'
        << "wrdata samples.dat v(vin) v(vout)\nquit\n.endc\n.end\n";
    return out.str();
}
Samples read_ngspice_table(std::istream& in) {
    std::string line, a, b, c, extra;
    if (!std::getline(in, line)) throw std::runtime_error("empty ngspice output");
    std::istringstream header(line);
    if (!(header >> a >> b >> c) || (header >> extra) || a != "time" || b != "v(vin)" || c != "v(vout)")
        throw std::runtime_error("unexpected ngspice table header");
    Samples samples;
    while (std::getline(in, line)) {
        std::istringstream row(line); row.imbue(std::locale::classic()); Sample s{};
        if (!(row >> s.time_s >> s.vin_V >> s.vout_V) || (row >> extra))
            throw std::runtime_error("malformed ngspice numerical row");
        samples.push_back(s);
    }
    if (in.bad()) throw std::runtime_error("ngspice result read failure");
    check_samples(samples); return samples;
}
static std::string read_text(const fs::path& p) {
    std::ifstream in(p); if (!in) throw std::runtime_error("missing backend file: " + p.string());
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
std::string backend_version(const fs::path& exe, const fs::path& dir) {
    const auto log = fs::absolute(dir / "version.txt");
    if (run_process(exe, {"-n", "--version"}, fs::absolute(dir), log) != 0)
        throw std::runtime_error("ngspice version query failed; see version.txt");
    const auto version = read_text(log);
    if (version.find("ngspice-") == std::string::npos) throw std::runtime_error("executable did not identify as ngspice");
    return version;
}
Samples run_ngspice(const RcConfig& c, const fs::path& exe, const fs::path& directory) {
    check_config(c); // Reject before even opening a netlist or launching a process.
    const auto dir = fs::absolute(directory);
    if (fs::exists(dir / "samples.dat") || fs::exists(dir / "circuit.cir"))
        throw std::runtime_error("refusing to reuse backend artifacts");
    std::ofstream netlist(dir / "circuit.cir"); netlist << rc_netlist(c); netlist.close();
    if (!netlist) throw std::runtime_error("netlist write failed");
    const int code = run_process(exe, {"-n", "-b", "circuit.cir"}, dir, dir / "ngspice.log");
    if (code != 0) throw std::runtime_error("ngspice exit status " + std::to_string(code) + "; see ngspice.log");
    auto log = read_text(dir / "ngspice.log");
    std::transform(log.begin(), log.end(), log.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const auto* word : {"error", "fatal", "warning", "timestep too small", "aborted"})
        if (log.find(word) != std::string::npos) throw std::runtime_error("ngspice diagnostic requires review; see ngspice.log");
    std::ifstream table(dir / "samples.dat");
    if (!table) throw std::runtime_error("missing ngspice result file samples.dat");
    return read_ngspice_table(table);
}
}
