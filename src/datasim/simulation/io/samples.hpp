#pragma once
#include <istream>
#include <ostream>
#include <vector>
namespace datasim {
struct Sample { double time_s, vin_V, vout_V; };
using Samples = std::vector<Sample>;
void check_samples(const Samples&);
Samples read_csv(std::istream&);
void write_csv(std::ostream&, const Samples&);
}
