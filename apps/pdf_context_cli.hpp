#pragma once
#include "datasim/pdf/table_extraction.hpp"
#include <charconv>
#include <sstream>
#include <stdexcept>

namespace datasim::pdf::cli {
// BOX|PARENT|ROW:CHILD[|ROW:CHILD...] ; pipes cannot occur in source text.
inline ParentContextBinding parse_parent_context(const std::string& spec) {
    std::istringstream stream(spec);
    std::string box_text, parent, child;
    if (!std::getline(stream, box_text, '|') || !std::getline(stream, parent, '|'))
        throw std::invalid_argument("invalid parent context specification");
    std::istringstream coords(box_text);
    double values[4];
    for (double& value : values) {
        std::string token;
        if (!std::getline(coords, token, ',')) throw std::invalid_argument("invalid parent context box");
        std::size_t consumed = 0;
        value = std::stod(token, &consumed);
        if (consumed != token.size()) throw std::invalid_argument("invalid parent context coordinate");
    }
    if (coords.peek() != std::char_traits<char>::eof() || box_text.back() == ',')
        throw std::invalid_argument("invalid parent context box");
    ParentContextBinding binding{{values[0], values[1], values[2], values[3]}, parent, {}};
    while (std::getline(stream, child, '|')) {
        const auto colon = child.find(':');
        if (colon == std::string::npos) throw std::invalid_argument("invalid parent context child");
        std::size_t row = 0;
        const auto result = std::from_chars(child.data(), child.data() + colon, row);
        if (result.ec != std::errc{} || result.ptr != child.data() + colon)
            throw std::invalid_argument("invalid parent context row index");
        binding.children.push_back({row, child.substr(colon + 1U)});
    }
    if (spec.back() == '|') throw std::invalid_argument("empty parent context child");
    return binding;
}
} // namespace datasim::pdf::cli
