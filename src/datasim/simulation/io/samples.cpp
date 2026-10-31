#include "samples.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
namespace datasim {
void check_samples(const Samples& s) {
    if (s.empty()) throw std::runtime_error("empty simulation results");
    double last = -1;
    for (const auto& x : s) {
        if (!std::isfinite(x.time_s) || !std::isfinite(x.vin_V) || !std::isfinite(x.vout_V) ||
            x.time_s < 0 || x.time_s <= last)
            throw std::runtime_error("non-finite values or non-increasing time");
        last = x.time_s;
    }
}
Samples read_csv(std::istream& in) {
    std::string line;
    if (!std::getline(in, line)) throw std::runtime_error("missing CSV header");
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != "time_s,vin_V,vout_V") throw std::runtime_error("unexpected CSV header");
    Samples result;
    while (std::getline(in, line)) {
        if (std::count(line.begin(), line.end(), ',') != 2) throw std::runtime_error("malformed CSV row");
        std::istringstream row(line);
        Sample s{};
        for (double* value : {&s.time_s, &s.vin_V, &s.vout_V}) {
            std::string cell, extra;
            if (!std::getline(row, cell, ',')) throw std::runtime_error("missing CSV cell");
            std::istringstream number(cell); number.imbue(std::locale::classic());
            if (!(number >> *value) || (number >> extra))
                throw std::runtime_error("malformed numerical CSV cell");
        }
        result.push_back(s);
    }
    if (in.bad()) throw std::runtime_error("CSV read failure");
    check_samples(result); return result;
}
void write_csv(std::ostream& out, const Samples& s) {
    check_samples(s); out.imbue(std::locale::classic()); out << std::setprecision(17);
    out << "time_s,vin_V,vout_V\n";
    for (const auto& x : s) out << x.time_s << ',' << x.vin_V << ',' << x.vout_V << '\n';
    if (!out) throw std::runtime_error("CSV write failure");
}
}
