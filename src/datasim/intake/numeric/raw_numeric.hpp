#pragma once

#include <optional>
#include <string_view>

namespace datasim::intake {

// Parses only a complete, locale-independent decimal/scientific scalar.
[[nodiscard]] std::optional<double> parse_unambiguous_scalar(std::string_view text) noexcept;
[[nodiscard]] bool is_ambiguous_numeric_text(std::string_view text) noexcept;

} // namespace datasim::intake
