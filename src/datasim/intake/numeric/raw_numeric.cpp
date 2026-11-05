#include "datasim/intake/numeric/raw_numeric.hpp"
#include <charconv>
#include <cmath>
#include <string>

namespace datasim::intake {

std::optional<double> parse_unambiguous_scalar(std::string_view text) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    double value = 0.0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

bool is_ambiguous_numeric_text(std::string_view text) noexcept {
    return !text.empty() && !parse_unambiguous_scalar(text).has_value();
}

} // namespace datasim::intake
