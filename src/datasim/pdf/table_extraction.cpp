#include "datasim/pdf/table_extraction.hpp"
#include "datasim/intake/numeric/raw_numeric.hpp"
#include "process_capture.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cctype>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace datasim::pdf {
namespace {
struct Word { double x0, y0, x1, y1; std::string text; };
struct Row { double y = 0.0; std::vector<Word> words; };
std::atomic<unsigned long long> temp_counter{0};

std::string decode_html(std::string text) {
    const std::pair<std::string, std::string> entities[] = {
        {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"}};
    for (const auto& [encoded, decoded] : entities) {
        std::size_t at = 0;
        while ((at = text.find(encoded, at)) != std::string::npos) {
            text.replace(at, encoded.size(), decoded);
            at += decoded.size();
        }
    }
    return text;
}

double attribute_number(const std::string& attributes, const char* key) {
    const std::regex expression(std::string("\\b") + key + "=\"([0-9.+-]+)\"");
    std::smatch match;
    if (!std::regex_search(attributes, match, expression)) throw std::runtime_error("missing PDF word coordinate");
    return std::stod(match[1].str());
}

std::vector<Word> parse_words(const std::string& html) {
    const std::regex expression(R"(<word\s+([^>]*)>(.*?)</word>)", std::regex::icase);
    std::vector<Word> result;
    for (auto it = std::sregex_iterator(html.begin(), html.end(), expression);
         it != std::sregex_iterator(); ++it) {
        if (result.size() >= 100000U) throw std::runtime_error("selected PDF page contains too many text words");
        const auto match = *it;
        const auto attributes = match[1].str();
        result.push_back({attribute_number(attributes, "xMin"), attribute_number(attributes, "yMin"),
                          attribute_number(attributes, "xMax"), attribute_number(attributes, "yMax"),
                          decode_html(match[2].str())});
    }
    return result;
}

bool contains_x(HorizontalSpan span, double x) { return x >= span.min && x < span.max; }

std::string join_words(std::vector<Word> words) {
    std::sort(words.begin(), words.end(), [](const Word& a, const Word& b) {
        if (a.x0 != b.x0) return a.x0 < b.x0;
        if (a.x1 != b.x1) return a.x1 < b.x1;
        return a.text < b.text;
    });
    std::string result;
    for (const auto& word : words) {
        if (!result.empty()) result.push_back(' ');
        result += word.text;
    }
    return result;
}

std::string safe_id(std::string value) {
    for (auto& ch : value) {
        const auto byte = static_cast<unsigned char>(ch);
        if (!std::isalnum(byte) && ch != '_' && ch != '-' && ch != '.') ch = '_';
    }
    return value;
}

std::string path_utf8(const std::filesystem::path& path) {
    const auto encoded = path.u8string();
    return {reinterpret_cast<const char*>(encoded.data()), encoded.size()};
}

std::optional<component::OperatingCondition> parse_condition(std::string_view text) {
    const auto equals = text.find('=');
    if (equals == std::string_view::npos) return std::nullopt;
    auto trim = [](std::string value) {
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string::npos) return std::string{};
        const auto last = value.find_last_not_of(" \t");
        return value.substr(first, last - first + 1U);
    };
    const auto name = trim(std::string(text.substr(0, equals)));
    const auto right = trim(std::string(text.substr(equals + 1U)));
    const auto split = right.find_first_of(" \t");
    if (name.empty() || split == std::string::npos) return std::nullopt;
    const auto value = right.substr(0, split);
    const auto unit = trim(right.substr(split + 1U));
    if (unit.empty()) return std::nullopt;
    auto number = intake::parse_unambiguous_scalar(value);
    if (!number.has_value()) return std::nullopt;
    return component::OperatingCondition{name, value, unit, number, std::nullopt};
}

std::optional<std::vector<component::OperatingCondition>> parse_conditions(std::string_view text) {
    if (text.empty()) return std::vector<component::OperatingCondition>{};
    std::vector<component::OperatingCondition> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(';', start);
        const auto segment = text.substr(start, end == std::string_view::npos ? end : end - start);
        const auto condition = parse_condition(segment);
        if (!condition.has_value()) return std::nullopt;
        result.push_back(*condition);
        if (end == std::string_view::npos) break;
        start = end + 1U;
    }
    return result;
}

bool is_single_token(std::string_view value) {
    return !value.empty() && value.find_first_of(" \t\r\n") == std::string_view::npos;
}

bool has_whitespace_delimited_token(std::string_view text, std::string_view token) {
    if (token.empty()) return false;
    std::size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string_view::npos) {
        const bool left_ok = (pos == 0) || std::isspace(static_cast<unsigned char>(text[pos - 1U]));
        const std::size_t end = pos + token.size();
        const bool right_ok = (end == text.size()) || std::isspace(static_cast<unsigned char>(text[end]));
        if (left_ok && right_ok) return true;
        pos += 1U;
    }
    return false;
}

bool header_contains_unit_symbol(std::string_view header_text, std::string_view unit_symbol) {
    if (!is_single_token(unit_symbol)) return false;
    if (header_text == unit_symbol) return true;
    const std::string paren = "(" + std::string(unit_symbol) + ")";
    const std::string bracket = "[" + std::string(unit_symbol) + "]";
    return has_whitespace_delimited_token(header_text, paren) ||
           has_whitespace_delimited_token(header_text, bracket);
}

HorizontalSpan target_span_for_qualifier(const ElectricalTableLayout& t, component::ValueQualifier qualifier) {
    switch (qualifier) {
        case component::ValueQualifier::minimum: return t.minimum_column;
        case component::ValueQualifier::typical: return t.typical_column;
        case component::ValueQualifier::maximum: return t.maximum_column;
        default: throw std::invalid_argument("header unit binding qualifier must be minimum, typical, or maximum");
    }
}

std::string column_name_for_qualifier(component::ValueQualifier qualifier) {
    switch (qualifier) {
        case component::ValueQualifier::minimum: return "min";
        case component::ValueQualifier::typical: return "typ";
        case component::ValueQualifier::maximum: return "max";
        default: throw std::invalid_argument("unsupported value qualifier");
    }
}

bool is_valid_inline_unit_token(std::string_view symbol) {
    if (!is_single_token(symbol)) return false;
    const auto first = static_cast<unsigned char>(symbol.front());
    if (std::isdigit(first) || first == '+' || first == '-' || first == '.' || first == '*') {
        return false;
    }
    bool has_unit_char = false;
    for (const char ch : symbol) {
        const auto byte = static_cast<unsigned char>(ch);
        if (std::iscntrl(byte)) return false;
        if (ch == '*' || ch == ',' || ch == ';' || ch == '=' ||
            ch == '(' || ch == ')' || ch == '[' || ch == ']') {
            return false;
        }
        if (std::isalpha(byte) || ch == '%' || byte >= 0x80U) {
            has_unit_char = true;
        }
    }
    return has_unit_char;
}

bool is_locale_ambiguous_number(std::string_view token) {
    if (token.empty()) return false;
    std::size_t pos = 0;
    if (token[0] == '+' || token[0] == '-') {
        pos = 1U;
        if (pos >= token.size()) return false;
    }
    if (!std::isdigit(static_cast<unsigned char>(token[pos])) ||
        !std::isdigit(static_cast<unsigned char>(token.back()))) {
        return false;
    }
    bool has_comma = false;
    bool prev_comma = false;
    for (; pos < token.size(); ++pos) {
        const char ch = token[pos];
        if (ch == ',') {
            if (prev_comma) return false;
            has_comma = true;
            prev_comma = true;
        } else if (std::isdigit(static_cast<unsigned char>(ch))) {
            prev_comma = false;
        } else {
            return false;
        }
    }
    return has_comma;
}

bool is_supported_inline_value_token(std::string_view token, bool allow_footnote) {
    if (intake::parse_unambiguous_scalar(token).has_value()) return true;
    if (allow_footnote && token.size() > 1U && token.back() == '*' &&
        intake::parse_unambiguous_scalar(token.substr(0, token.size() - 1U)).has_value()) {
        return true;
    }
    if (allow_footnote && is_locale_ambiguous_number(token)) return true;
    return false;
}

struct ParsedInlineCell {
    std::string value_token;
    std::optional<std::string> unit_token;
    bool unit_has_footnote = false;
};

ParsedInlineCell parse_inline_value_unit_cell(const std::string& raw_cell_text,
                                              const InlineValueUnitBinding& binding,
                                              std::size_t row_number) {
    std::istringstream stream(raw_cell_text);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() > 2U) {
        throw std::runtime_error("row " + std::to_string(row_number) +
            " inline value cell '" + raw_cell_text +
            "' contains unsupported multi-token representation");
    }
    if (tokens.size() == 2U) {
        const std::string& val_tok = tokens[0];
        const std::string& unit_cand = tokens[1];
        std::optional<std::string> inline_unit;
        bool unit_footnote = false;
        if (std::find(binding.allowed_unit_symbols.begin(),
                      binding.allowed_unit_symbols.end(),
                      unit_cand) != binding.allowed_unit_symbols.end()) {
            inline_unit = unit_cand;
        } else if (unit_cand.size() > 1U && unit_cand.back() == '*') {
            const std::string stripped = unit_cand.substr(0, unit_cand.size() - 1U);
            if (std::find(binding.allowed_unit_symbols.begin(),
                          binding.allowed_unit_symbols.end(),
                          stripped) != binding.allowed_unit_symbols.end()) {
                inline_unit = stripped;
                unit_footnote = true;
            }
        }
        if (!inline_unit.has_value()) {
            throw std::runtime_error("row " + std::to_string(row_number) +
                " inline unit token '" + unit_cand +
                "' is not permitted by configured inline value/unit binding");
        }
        if (!is_supported_inline_value_token(val_tok, !unit_footnote)) {
            throw std::runtime_error("row " + std::to_string(row_number) +
                " inline value cell '" + raw_cell_text +
                "' contains unsupported value representation '" + val_tok + "'");
        }
        return {val_tok, inline_unit, unit_footnote};
    }
    const std::string& val_tok = tokens[0];
    if (!is_supported_inline_value_token(val_tok, true)) {
        throw std::runtime_error("row " + std::to_string(row_number) +
            " inline value cell '" + raw_cell_text +
            "' contains unsupported value representation '" + val_tok + "'");
    }
    return {val_tok, std::nullopt, false};
}

void validate_request(const ExtractionRequest& request) {
    const auto& t = request.table;
    if (request.pdf_path.empty() || request.pdfinfo_path.empty() || request.pdftotext_path.empty())
        throw std::invalid_argument("PDF path and both Poppler executable paths are required");
    if (!t.category.has_value()) throw std::invalid_argument("parameter category must be explicit; no rating category is inferred");
    if (t.table_id.empty() || !(t.region_x_min < t.region_x_max) || !(t.region_y_min < t.region_y_max) ||
        !(t.row_y_tolerance >= 0.0) || !(t.header_y_min <= t.header_y_max))
        throw std::invalid_argument("invalid explicit table region or row configuration");
    const HorizontalSpan spans[] = {t.parameter_column, t.unit_column, t.minimum_column,
                                    t.typical_column, t.maximum_column};
    for (const auto span : spans) if (!(span.min < span.max)) throw std::invalid_argument("invalid column span");
    for (std::size_t i = 0; i < std::size(spans); ++i)
        for (std::size_t j = i + 1; j < std::size(spans); ++j)
            if (std::max(spans[i].min, spans[j].min) < std::min(spans[i].max, spans[j].max))
                throw std::invalid_argument("table column spans overlap");
    if (t.condition_column.has_value()) {
        if (!(t.condition_column->min < t.condition_column->max))
            throw std::invalid_argument("invalid condition column span");
        for (const auto span : spans)
            if (std::max(span.min, t.condition_column->min) < std::min(span.max, t.condition_column->max))
                throw std::invalid_argument("condition column overlaps another column");
    }
    if (!t.header_unit_bindings.empty() || t.header_condition_binding.has_value()) {
        if (!(t.header_y_min < t.header_y_max) || t.header_y_min < t.region_y_min || t.header_y_max > t.region_y_max)
            throw std::invalid_argument("header bindings require a non-empty header y-range inside the table region");
    }
    std::vector<component::ValueQualifier> bound_qualifiers;
    for (const auto& binding : t.header_unit_bindings) {
        const auto target_span = target_span_for_qualifier(t, binding.qualifier);
        if (std::find(bound_qualifiers.begin(), bound_qualifiers.end(), binding.qualifier) != bound_qualifiers.end())
            throw std::invalid_argument("duplicate header unit binding for the same value column");
        bound_qualifiers.push_back(binding.qualifier);
        if (!(binding.header_x_span.min < binding.header_x_span.max) || !(binding.header_y_min < binding.header_y_max))
            throw std::invalid_argument("invalid header unit binding box coordinates");
        if (binding.header_x_span.min < t.region_x_min || binding.header_x_span.max > t.region_x_max ||
            binding.header_y_min < t.header_y_min || binding.header_y_max > t.header_y_max)
            throw std::invalid_argument("header unit binding box must lie within table region and header y-range");
        if (!(std::max(binding.header_x_span.min, target_span.min) < std::min(binding.header_x_span.max, target_span.max)))
            throw std::invalid_argument("header unit binding x-span does not overlap target value column");
        if (binding.expected_header_text.empty() || !header_contains_unit_symbol(binding.expected_header_text, binding.unit_symbol))
            throw std::invalid_argument("header unit binding expected_header_text must explicitly contain unit_symbol");
    }
    if (t.header_condition_binding.has_value()) {
        if (!t.condition_column.has_value())
            throw std::invalid_argument("header condition binding requires condition_column");
        const auto& b = *t.header_condition_binding;
        if (!(b.header_x_span.min < b.header_x_span.max) || !(b.header_y_min < b.header_y_max))
            throw std::invalid_argument("invalid header condition binding box coordinates");
        if (b.header_x_span.min < t.region_x_min || b.header_x_span.max > t.region_x_max ||
            b.header_y_min < t.header_y_min || b.header_y_max > t.header_y_max)
            throw std::invalid_argument("header condition binding box must lie within table region and header y-range");
        if (!(std::max(b.header_x_span.min, t.condition_column->min) < std::min(b.header_x_span.max, t.condition_column->max)))
            throw std::invalid_argument("header condition binding x-span does not overlap condition_column");
        if (b.expected_header_text.empty() || !is_single_token(b.parameter_name) ||
            !has_whitespace_delimited_token(b.expected_header_text, b.parameter_name) ||
            !header_contains_unit_symbol(b.expected_header_text, b.unit_symbol))
            throw std::invalid_argument("header condition binding expected_header_text must contain parameter_name and unit_symbol");
    }
    std::vector<component::ValueQualifier> inline_bound_qualifiers;
    for (const auto& binding : t.inline_value_unit_bindings) {
        (void)target_span_for_qualifier(t, binding.qualifier);
        if (std::find(inline_bound_qualifiers.begin(), inline_bound_qualifiers.end(), binding.qualifier) !=
            inline_bound_qualifiers.end()) {
            throw std::invalid_argument("duplicate inline value/unit binding for the same value column");
        }
        inline_bound_qualifiers.push_back(binding.qualifier);
        if (binding.allowed_unit_symbols.empty()) {
            throw std::invalid_argument("inline value/unit binding requires at least one allowed_unit_symbol");
        }
        std::vector<std::string> seen_units;
        for (const auto& unit_symbol : binding.allowed_unit_symbols) {
            if (!is_valid_inline_unit_token(unit_symbol)) {
                throw std::invalid_argument("invalid allowed_unit_symbol in inline value/unit binding: " + unit_symbol);
            }
            if (std::find(seen_units.begin(), seen_units.end(), unit_symbol) != seen_units.end()) {
                throw std::invalid_argument("duplicate allowed_unit_symbol in inline value/unit binding: " + unit_symbol);
            }
            seen_units.push_back(unit_symbol);
        }
    }
}

void verify_header_box_text(const std::vector<Word>& page_words, HorizontalSpan x_span,
                            double y_min, double y_max, double row_y_tolerance,
                            const std::string& expected_text) {
    std::vector<Word> box_words;
    for (const auto& word : page_words) {
        const bool overlaps_x = std::max(word.x0, x_span.min) < std::min(word.x1, x_span.max);
        const bool overlaps_y = std::max(word.y0, y_min) < std::min(word.y1, y_max);
        if (!overlaps_x || !overlaps_y) continue;
        if (word.x0 < x_span.min || word.x1 > x_span.max || word.y0 < y_min || word.y1 > y_max)
            throw std::runtime_error("header word straddles configured header binding box: " + word.text);
        box_words.push_back(word);
    }
    if (box_words.empty())
        throw std::runtime_error("configured header binding box contains no selectable text");
    std::sort(box_words.begin(), box_words.end(), [](const Word& a, const Word& b) {
        if (a.y0 != b.y0) return a.y0 < b.y0;
        if (a.x0 != b.x0) return a.x0 < b.x0;
        return a.text < b.text;
    });
    std::vector<Row> lines;
    for (const auto& word : box_words) {
        const double center = (word.y0 + word.y1) / 2.0;
        if (lines.empty() || std::abs(center - lines.back().y) > row_y_tolerance)
            lines.push_back({center, {word}});
        else {
            auto& line = lines.back();
            line.y = (line.y * static_cast<double>(line.words.size()) + center) /
                     static_cast<double>(line.words.size() + 1U);
            line.words.push_back(word);
        }
    }
    std::string actual;
    for (const auto& line : lines) {
        if (!actual.empty()) actual.push_back(' ');
        actual += join_words(line.words);
    }
    if (actual != expected_text)
        throw std::runtime_error("header binding text mismatch: expected '" + expected_text +
                                 "' but found '" + actual + "'");
}

std::string tool_version(const std::filesystem::path& tool, const std::filesystem::path& temp) {
    const auto output = temp / "version.txt";
    if (detail::run_capture(tool, {"-v"}, output) != 0) throw std::runtime_error("Poppler version check failed");
    auto version = detail::read_text_file(output);
    std::replace(version.begin(), version.end(), '\r', ' ');
    std::replace(version.begin(), version.end(), '\n', ' ');
    while (!version.empty() && std::isspace(static_cast<unsigned char>(version.back()))) version.pop_back();
    return version;
}

std::string row_cell(const std::vector<Word>& words, HorizontalSpan span) {
    std::vector<Word> selected;
    for (const auto& word : words) if (contains_x(span, (word.x0 + word.x1) / 2.0)) selected.push_back(word);
    return join_words(std::move(selected));
}

std::string row_value_cell(const std::vector<Word>& words, HorizontalSpan span, std::size_t row_number) {
    std::vector<Word> selected;
    for (const auto& word : words) {
        if (contains_x(span, (word.x0 + word.x1) / 2.0)) selected.push_back(word);
    }
    std::sort(selected.begin(), selected.end(), [](const Word& a, const Word& b) {
        if (a.x0 != b.x0) return a.x0 < b.x0;
        if (a.x1 != b.x1) return a.x1 < b.x1;
        return a.text < b.text;
    });
    for (std::size_t i = 0; i + 1U < selected.size(); ++i) {
        if (selected[i + 1U].x0 < selected[i].x1) {
            throw std::runtime_error("row " + std::to_string(row_number) +
                " value cell has overlapping or duplicate word bounding boxes");
        }
    }
    std::string result;
    for (const auto& word : selected) {
        if (!result.empty()) result.push_back(' ');
        result += word.text;
    }
    return result;
}

struct TempDirectory {
    std::filesystem::path path;
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

bool overlaps(const SourceBox& a, const SourceBox& b) {
    return std::max(a.x_min, b.x_min) < std::min(a.x_max, b.x_max) &&
           std::max(a.y_min, b.y_min) < std::min(a.y_max, b.y_max);
}

SourceBox word_box(const Word& w) { return {w.x0, w.y0, w.x1, w.y1}; }

bool inside(const SourceBox& a, const SourceBox& b) {
    return a.x_min >= b.x_min && a.y_min >= b.y_min &&
           a.x_max <= b.x_max && a.y_max <= b.y_max;
}

SourceBox row_box(const Row& row) {
    auto box = word_box(row.words.front());
    for (const auto& w : row.words) {
        box.x_min = std::min(box.x_min, w.x0); box.y_min = std::min(box.y_min, w.y0);
        box.x_max = std::max(box.x_max, w.x1); box.y_max = std::max(box.y_max, w.y1);
    }
    return box;
}

std::vector<Word> without_parents(const Row& row, const ElectricalTableLayout& t) {
    std::vector<Word> words;
    for (const auto& w : row.words) {
        if (std::none_of(t.parent_context_bindings.begin(), t.parent_context_bindings.end(),
                [&](const auto& b) { return inside(word_box(w), b.parent_box); }))
            words.push_back(w);
    }
    return words;
}

bool has_values(const Row& row, const ElectricalTableLayout& t) {
    for (const auto span : {t.minimum_column, t.typical_column, t.maximum_column}) {
        const auto text = row_cell(row.words, span);
        if (!text.empty() && text != "-" && text != "\xE2\x80\x94") return true;
    }
    return false;
}

// Verify all associations before emitting any observations. No best-effort fallback.
std::vector<std::optional<std::size_t>> verify_parent_context(
        const std::vector<Word>& words, const std::vector<Row>& rows,
        const ElectricalTableLayout& t) {
    std::vector<std::optional<std::size_t>> owners(rows.size());
    const SourceBox region{t.region_x_min, t.region_y_min, t.region_x_max, t.region_y_max};
    for (std::size_t i = 0; i < t.parent_context_bindings.size(); ++i) {
        const auto& b = t.parent_context_bindings[i];
        const auto& box = b.parent_box;
        if (!std::isfinite(box.x_min) || !std::isfinite(box.y_min) ||
            !std::isfinite(box.x_max) || !std::isfinite(box.y_max) ||
            !(box.x_min < box.x_max) || !(box.y_min < box.y_max) || !inside(box, region) ||
            box.x_min < t.parameter_column.min || box.x_max > t.parameter_column.max ||
            (std::max(box.y_min, t.header_y_min) < std::min(box.y_max, t.header_y_max)))
            throw std::runtime_error("parent context box must be finite and inside the non-header parameter region");
        if (b.expected_parent_text.empty() || b.children.empty())
            throw std::runtime_error("parent context requires exact text and explicit children");
        for (std::size_t j = 0; j < i; ++j)
            if (overlaps(box, t.parent_context_bindings[j].parent_box))
                throw std::runtime_error("parent context boxes overlap");
        std::vector<Word> source;
        for (const auto& w : words) {
            if (!overlaps(word_box(w), box)) continue;
            if (!inside(word_box(w), box))
                throw std::runtime_error("parent context box cuts a source word");
            source.push_back(w);
        }
        if (source.empty() || join_words(source) != b.expected_parent_text)
            throw std::runtime_error("parent context text mismatch");
        double low = (source.front().y0 + source.front().y1) / 2.0, high = low;
        for (std::size_t j = 0; j < source.size(); ++j) {
            const double y = (source[j].y0 + source[j].y1) / 2.0;
            low = std::min(low, y); high = std::max(high, y);
            for (std::size_t k = 0; k < j; ++k)
                if (std::max(source[j].x0, source[k].x0) < std::min(source[j].x1, source[k].x1))
                    throw std::runtime_error("parent context has overlapping source words");
        }
        if (high - low > t.row_y_tolerance)
            throw std::runtime_error("parent context must have a single text baseline");
        bool merged = false;
        std::vector<std::size_t> targets;
        for (const auto& child : b.children) {
            if (child.row_index == 0 || child.row_index > rows.size())
                throw std::runtime_error("parent context child row is outside selected scope");
            const auto r = child.row_index - 1U;
            if (owners[r].has_value()) throw std::runtime_error("parent context has competing child assignments");
            owners[r] = i;
            targets.push_back(r);
            const auto& row = rows[r];
            if (!has_values(row, t)) throw std::runtime_error("parent context child has no value cell");
            const auto raw = row_cell(without_parents(row, t), t.parameter_column);
            if (raw != child.expected_parameter_text)
                throw std::runtime_error("parent context child text mismatch");
            const auto rb = row_box(row);
            if (box.y_max <= rb.y_min) {
                for (std::size_t between = 0; between < r; ++between) {
                    const auto middle = row_box(rows[between]);
                    if (middle.y_min >= box.y_max && !has_values(rows[between], t) &&
                        !row_cell(rows[between].words, t.parameter_column).empty())
                        throw std::runtime_error("parent context crosses an intervening parameter heading");
                }
            }
            if (box.y_max > rb.y_min) {
                merged = true;
                if (!raw.empty() || rb.y_min < box.y_min || rb.y_max > box.y_max)
                    throw std::runtime_error("parent context merged cell has competing text or partial child geometry");
            }
        }
        std::sort(targets.begin(), targets.end());
        if (merged) {
            for (std::size_t j = 0; j < targets.size(); ++j) {
                const auto rb = row_box(rows[targets[j]]);
                if (rb.y_min < box.y_min || rb.y_max > box.y_max ||
                    !row_cell(without_parents(rows[targets[j]], t), t.parameter_column).empty() ||
                    (j > 0 && targets[j] != targets[j - 1U] + 1U))
                    throw std::runtime_error("parent context merged children must be consecutive and wholly bounded");
            }
        }
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto rb = row_box(rows[r]);
            if (std::max(rb.y_min, box.y_min) < std::min(rb.y_max, box.y_max) &&
                has_values(rows[r], t) && owners[r] != i)
                throw std::runtime_error("parent context box touches an unassigned value row");
        }
    }
    return owners;
}
}

ExtractionResult extract_electrical_table(const ExtractionRequest& request) {
    ExtractionResult result;
    try {
        validate_request(request);
        TempDirectory temp{std::filesystem::temp_directory_path() /
            ("datasim_pdf_" + std::to_string(temp_counter.fetch_add(1)))};
        std::filesystem::create_directories(temp.path);
        const auto info_file = temp.path / "pdfinfo.txt";
        if (detail::run_capture(request.pdfinfo_path, {path_utf8(request.pdf_path)}, info_file) != 0)
            throw std::runtime_error("pdfinfo could not read the selected PDF");
        const auto info = detail::read_text_file(info_file);
        std::smatch page_match;
        if (!std::regex_search(info, page_match, std::regex(R"((?:^|\n)Pages:\s+([0-9]+))")))
            throw std::runtime_error("pdfinfo output did not contain page count");
        const auto page_count = static_cast<std::size_t>(std::stoull(page_match[1].str()));
        if (page_count == 0 || request.pdf_page_index >= page_count)
            throw std::out_of_range("selected zero-based PDF page index is outside the document");
        const auto text_file = temp.path / "page.html";
        const auto page_one_based = std::to_string(request.pdf_page_index + 1U);
        if (detail::run_capture(request.pdftotext_path,
                {"-bbox-layout", "-f", page_one_based, "-l", page_one_based,
                 path_utf8(request.pdf_path), path_utf8(text_file)}, temp.path / "pdftotext.log") != 0)
            throw std::runtime_error("pdftotext failed to extract selectable text");
        if (std::filesystem::file_size(text_file) > 32U * 1024U * 1024U)
            throw std::runtime_error("selected-page text extraction exceeds the 32 MiB safety limit");
        const auto html = detail::read_text_file(text_file);
        const auto words = parse_words(html);
        if (words.empty()) throw std::runtime_error("selected page has no selectable text; image-only pages are unsupported");

        std::vector<Word> selected;
        for (const auto& word : words) {
            if (word.x0 >= request.table.region_x_min && word.x1 <= request.table.region_x_max &&
                word.y0 >= request.table.region_y_min && word.y1 <= request.table.region_y_max)
                selected.push_back(word);
        }
        if (selected.empty()) throw std::runtime_error("explicit table region contains no selectable text");
        std::sort(selected.begin(), selected.end(), [](const Word& a, const Word& b) {
            if (a.y0 != b.y0) return a.y0 < b.y0;
            if (a.x0 != b.x0) return a.x0 < b.x0;
            return a.text < b.text;
        });
        for (const auto& binding : request.table.header_unit_bindings) {
            verify_header_box_text(words, binding.header_x_span, binding.header_y_min,
                                   binding.header_y_max, request.table.row_y_tolerance,
                                   binding.expected_header_text);
        }
        if (request.table.header_condition_binding.has_value()) {
            const auto& b = *request.table.header_condition_binding;
            verify_header_box_text(words, b.header_x_span, b.header_y_min, b.header_y_max,
                                   request.table.row_y_tolerance, b.expected_header_text);
        }

        std::vector<Row> rows;
        for (const auto& word : selected) {
            if (word.y0 >= request.table.header_y_min && word.y1 <= request.table.header_y_max) continue;
            const double center = (word.y0 + word.y1) / 2.0;
            if (rows.empty() || std::abs(center - rows.back().y) > request.table.row_y_tolerance)
                rows.push_back({center, {word}});
            else {
                auto& row = rows.back();
                row.y = (row.y * static_cast<double>(row.words.size()) + center) /
                        static_cast<double>(row.words.size() + 1U);
                row.words.push_back(word);
            }
        }
        if (rows.empty()) throw std::runtime_error("table selection contains only the configured header row");
        const auto context_owners = verify_parent_context(words, rows, request.table);

        intake::IntakeManifest manifest;
        manifest.origin = request.component.is_synthetic ? intake::IntakeOriginClass::synthetic_fixture
                                                          : intake::IntakeOriginClass::manual_curation;
        manifest.curator = request.curator;
        manifest.component = request.component;
        manifest.source_document = request.source_document;
        manifest.declared_pdf_page_count = page_count;
        const auto version = tool_version(request.pdftotext_path, temp.path);

        struct VerifiedUnitHeader {
            HeaderUnitBinding binding;
            component::EvidenceLocation evidence;
            bool registered = false;
        };
        std::vector<VerifiedUnitHeader> unit_headers;
        unit_headers.reserve(request.table.header_unit_bindings.size());
        for (const auto& binding : request.table.header_unit_bindings) {
            const std::string col_name = column_name_for_qualifier(binding.qualifier);
            const std::string hdr_base = safe_id(request.source_document.id.value + "." +
                request.table.table_id + ".p" + std::to_string(request.pdf_page_index) +
                ".header_unit." + col_name);
            component::EvidenceLocation hdr_ev;
            hdr_ev.id = {"ev." + hdr_base};
            hdr_ev.source_document_id = request.source_document.id;
            hdr_ev.pdf_page_index = request.pdf_page_index;
            hdr_ev.printed_page_label = request.printed_page_label;
            hdr_ev.table_or_figure_id = request.table.table_id;
            hdr_ev.cell_or_region = "header_unit;column=" + col_name;
            unit_headers.push_back({binding, std::move(hdr_ev), false});
        }
        std::optional<component::EvidenceLocation> cond_header_ev;
        bool cond_header_registered = false;
        if (request.table.header_condition_binding.has_value()) {
            const std::string hdr_base = safe_id(request.source_document.id.value + "." +
                request.table.table_id + ".p" + std::to_string(request.pdf_page_index) +
                ".header_condition");
            component::EvidenceLocation hdr_ev;
            hdr_ev.id = {"ev." + hdr_base};
            hdr_ev.source_document_id = request.source_document.id;
            hdr_ev.pdf_page_index = request.pdf_page_index;
            hdr_ev.printed_page_label = request.printed_page_label;
            hdr_ev.table_or_figure_id = request.table.table_id;
            hdr_ev.cell_or_region = "header_condition;column=condition";
            cond_header_ev = std::move(hdr_ev);
        }

        std::vector<ValueCellProvenance> cell_provenance;
        std::vector<ParameterContextProvenance> context_provenance;
        std::size_t row_number = 0;
        for (const auto& row : rows) {
            ++row_number;
            std::string parameter = row_cell(row.words, request.table.parameter_column);
            const auto owner = context_owners[row_number - 1U];
            std::string raw_child_parameter = parameter;
            if (!request.table.parent_context_bindings.empty()) {
                raw_child_parameter = row_cell(without_parents(row, request.table), request.table.parameter_column);
                parameter = raw_child_parameter;
                if (owner.has_value()) {
                    parameter = request.table.parent_context_bindings[*owner].expected_parent_text;
                    if (!raw_child_parameter.empty()) parameter += " " + raw_child_parameter;
                }
            }
            std::string unit = row_cell(row.words, request.table.unit_column);
            if (parameter.empty()) {
                result.diagnostics.push_back("row " + std::to_string(row_number) + ": missing parameter cell; row omitted");
                continue;
            }
            std::vector<HorizontalSpan> configured_spans{
                request.table.parameter_column, request.table.unit_column,
                request.table.minimum_column, request.table.typical_column,
                request.table.maximum_column};
            if (request.table.condition_column.has_value())
                configured_spans.push_back(*request.table.condition_column);
            for (const auto& word : row.words) {
                const auto matches = std::count_if(configured_spans.begin(), configured_spans.end(),
                    [&](HorizontalSpan span) {
                        return contains_x(span, (word.x0 + word.x1) / 2.0) &&
                               word.x0 >= span.min && word.x1 <= span.max;
                    });
                if (matches != 1)
                    throw std::runtime_error("row " + std::to_string(row_number) +
                        " has a word outside or ambiguously across configured column spans: " + word.text);
            }
            const std::string condition_text = request.table.condition_column.has_value()
                ? row_cell(row.words, *request.table.condition_column) : std::string{};
            for (const auto& [qualifier, column, column_name] : std::vector<std::tuple<component::ValueQualifier, HorizontalSpan, std::string>>{
                     {component::ValueQualifier::minimum, request.table.minimum_column, "min"},
                     {component::ValueQualifier::typical, request.table.typical_column, "typ"},
                     {component::ValueQualifier::maximum, request.table.maximum_column, "max"}}) {
                const std::string raw_cell_text = row_value_cell(row.words, column, row_number);
                if (raw_cell_text.empty() || raw_cell_text == "-" || raw_cell_text == std::string("\xE2\x80\x94")) continue;

                const InlineValueUnitBinding* inline_binding = nullptr;
                for (const auto& ib : request.table.inline_value_unit_bindings) {
                    if (ib.qualifier == qualifier) {
                        inline_binding = &ib;
                        break;
                    }
                }
                std::string value_token = raw_cell_text;
                std::optional<std::string> inline_unit_token;
                bool unit_has_footnote = false;
                if (inline_binding != nullptr) {
                    const auto parsed_inline = parse_inline_value_unit_cell(raw_cell_text, *inline_binding, row_number);
                    value_token = parsed_inline.value_token;
                    inline_unit_token = parsed_inline.unit_token;
                    unit_has_footnote = parsed_inline.unit_has_footnote;
                }

                const std::string base = safe_id(request.source_document.id.value + "." +
                    request.table.table_id + ".p" + std::to_string(request.pdf_page_index) +
                    ".r" + std::to_string(row_number) + "." + column_name);
                component::EvidenceLocation evidence;
                evidence.id = {"ev." + base};
                evidence.source_document_id = request.source_document.id;
                evidence.pdf_page_index = request.pdf_page_index;
                evidence.printed_page_label = request.printed_page_label;
                evidence.table_or_figure_id = request.table.table_id;
                evidence.cell_or_region = "row=" + std::to_string(row_number) + ";column=" + column_name;
                if (raw_cell_text.find('*') != std::string::npos) evidence.footnote_refs = {"*"};
                component::EvidenceLocation parameter_evidence = evidence;
                parameter_evidence.id = {"ev." + base + ".parameter"};
                parameter_evidence.cell_or_region = "row=" + std::to_string(row_number) + ";column=parameter/unit";
                parameter_evidence.footnote_refs.clear();
                manifest.evidence.push_back({parameter_evidence, raw_child_parameter, "parameter", false});
                manifest.evidence.push_back({evidence, parameter, column_name, false});
                std::optional<component::EvidenceLocation> parent_evidence;
                if (owner.has_value()) {
                    const auto& binding = request.table.parent_context_bindings[*owner];
                    parent_evidence = parameter_evidence;
                    parent_evidence->id = {"ev." + base + ".parent"};
                    parent_evidence->cell_or_region = "parent_context=" + std::to_string(*owner + 1U);
                    manifest.evidence.push_back({*parent_evidence, binding.expected_parent_text, "parent_parameter", false});
                    context_provenance.push_back({{"obs." + base}, parent_evidence->id,
                        parameter_evidence.id, row_number, binding.parent_box, row_box(row),
                        binding.expected_parent_text, raw_child_parameter, join_words(row.words)});
                }

                VerifiedUnitHeader* matched_unit_header = nullptr;
                for (auto& uh : unit_headers) {
                    if (uh.binding.qualifier == qualifier) {
                        matched_unit_header = &uh;
                        break;
                    }
                }
                std::optional<std::string> effective_unit;
                bool used_header_unit = false;
                if (inline_unit_token.has_value()) {
                    if (!unit.empty() && *inline_unit_token != unit) {
                        throw std::runtime_error("row " + std::to_string(row_number) +
                            " inline unit '" + *inline_unit_token + "' conflicts with explicit row unit '" + unit + "'");
                    }
                    if (matched_unit_header != nullptr && *inline_unit_token != matched_unit_header->binding.unit_symbol) {
                        throw std::runtime_error("row " + std::to_string(row_number) +
                            " inline unit '" + *inline_unit_token + "' conflicts with bound header unit '" +
                            matched_unit_header->binding.unit_symbol + "'");
                    }
                    effective_unit = *inline_unit_token;
                } else if (!unit.empty()) {
                    if (matched_unit_header != nullptr && unit != matched_unit_header->binding.unit_symbol) {
                        throw std::runtime_error("row " + std::to_string(row_number) +
                            " explicit unit '" + unit + "' conflicts with bound header unit '" +
                            matched_unit_header->binding.unit_symbol + "'");
                    }
                    if (inline_binding != nullptr &&
                        std::find(inline_binding->allowed_unit_symbols.begin(),
                                  inline_binding->allowed_unit_symbols.end(), unit) ==
                            inline_binding->allowed_unit_symbols.end()) {
                        throw std::runtime_error("row " + std::to_string(row_number) +
                            " explicit unit '" + unit + "' is not permitted by configured inline value/unit binding");
                    }
                    effective_unit = unit;
                } else if (matched_unit_header != nullptr) {
                    if (inline_binding != nullptr &&
                        std::find(inline_binding->allowed_unit_symbols.begin(),
                                  inline_binding->allowed_unit_symbols.end(),
                                  matched_unit_header->binding.unit_symbol) ==
                            inline_binding->allowed_unit_symbols.end()) {
                        throw std::runtime_error("row " + std::to_string(row_number) +
                            " bound header unit '" + matched_unit_header->binding.unit_symbol +
                            "' is not permitted by configured inline value/unit binding");
                    }
                    effective_unit = matched_unit_header->binding.unit_symbol;
                    used_header_unit = true;
                    if (!matched_unit_header->registered) {
                        manifest.evidence.push_back({matched_unit_header->evidence,
                                                     matched_unit_header->binding.expected_header_text,
                                                     column_name + "_header", false});
                        matched_unit_header->registered = true;
                    }
                }

                std::optional<std::vector<component::OperatingCondition>> resolved_conditions;
                bool used_header_condition = false;
                std::optional<std::string> condition_ambiguity_notes;
                if (condition_text.empty()) {
                    resolved_conditions = std::vector<component::OperatingCondition>{};
                } else if (condition_text.find('=') != std::string::npos) {
                    resolved_conditions = parse_conditions(condition_text);
                    if (request.table.header_condition_binding.has_value()) {
                        const auto& cb = *request.table.header_condition_binding;
                        if (!resolved_conditions.has_value())
                            throw std::runtime_error("row " + std::to_string(row_number) +
                                " inline operating condition conflicts with bound condition header");
                        for (const auto& cond : *resolved_conditions) {
                            if (cond.parameter_name != cb.parameter_name || cond.unit_symbol != cb.unit_symbol)
                                throw std::runtime_error("row " + std::to_string(row_number) +
                                    " inline operating condition conflicts with bound condition header");
                        }
                    }
                    if (!resolved_conditions.has_value()) {
                        condition_ambiguity_notes = "Condition text preserved verbatim but does not match the supported PARAMETER=value UNIT form.";
                    }
                } else if (request.table.header_condition_binding.has_value()) {
                    const auto& cb = *request.table.header_condition_binding;
                    const auto cond_scalar = intake::parse_unambiguous_scalar(condition_text);
                    if (cond_scalar.has_value()) {
                        resolved_conditions = std::vector<component::OperatingCondition>{
                            component::OperatingCondition{cb.parameter_name, condition_text,
                                                          cb.unit_symbol, cond_scalar, std::nullopt}};
                        used_header_condition = true;
                        if (!cond_header_registered) {
                            manifest.evidence.push_back({*cond_header_ev, cb.expected_header_text,
                                                         "condition_header", false});
                            cond_header_registered = true;
                        }
                    } else {
                        condition_ambiguity_notes = "Condition text preserved verbatim; bare cell is not an unambiguous scalar for the bound condition header.";
                    }
                } else {
                    condition_ambiguity_notes = "Condition text preserved verbatim but does not match the supported PARAMETER=value UNIT form.";
                }

                component::ParameterObservation observation;
                observation.id = {"obs." + base};
                observation.component_id = request.component.id;
                observation.parameter_name = parameter;
                observation.original_value_text = value_token;
                observation.original_unit_symbol = effective_unit;
                observation.unitless = false;
                observation.parsed_numeric_value = (effective_unit.has_value() && !unit_has_footnote)
                    ? intake::parse_unambiguous_scalar(value_token) : std::nullopt;
                observation.qualifier = qualifier;
                observation.category = *request.table.category;
                if (resolved_conditions.has_value()) observation.operating_conditions = *resolved_conditions;
                observation.evidence_refs = {parameter_evidence.id, evidence.id};
                if (parent_evidence.has_value()) observation.evidence_refs.push_back(parent_evidence->id);
                if (used_header_unit) observation.evidence_refs.push_back(matched_unit_header->evidence.id);
                if (used_header_condition) observation.evidence_refs.push_back(cond_header_ev->id);
                observation.extraction_method = component::ExtractionMethod::deterministic_table_parser;
                observation.extraction_tool = "Poppler pdftotext -bbox-layout";
                observation.extraction_tool_version = version;
                intake::IntakeObservationEntry entry;
                entry.observation = std::move(observation);
                if (!condition_text.empty()) {
                    entry.raw_conditions_text = condition_text;
                    if (!resolved_conditions.has_value()) {
                        entry.ambiguity_notes = condition_ambiguity_notes;
                        result.diagnostics.push_back("row " + std::to_string(row_number) + ": operating condition left unresolved: " + condition_text);
                    }
                }
                if (unit_has_footnote) {
                    const std::string fn_note = "Footnote-qualified inline value cell preserved without asserted numeric scalar.";
                    entry.ambiguity_notes = entry.ambiguity_notes.has_value()
                        ? (*entry.ambiguity_notes + " " + fn_note)
                        : std::optional<std::string>(fn_note);
                    result.diagnostics.push_back("row " + std::to_string(row_number) +
                        ": footnote-qualified inline value cell '" + raw_cell_text +
                        "'; numeric value was not asserted");
                }
                if (!effective_unit.has_value())
                    result.diagnostics.push_back("row " + std::to_string(row_number) +
                        ": unit cell is blank; no unitless status or numeric value was inferred");
                cell_provenance.push_back({
                    entry.observation.id,
                    evidence.id,
                    row_number,
                    qualifier,
                    raw_cell_text,
                    value_token,
                    inline_unit_token});
                manifest.observations.push_back(std::move(entry));
            }
        }
        if (manifest.observations.empty()) throw std::runtime_error("selected region contained no supported min/typ/max value cells");
        auto candidate = intake::build_candidate_bundle(manifest);
        result.report = std::move(candidate.report);
        result.manifest = std::move(manifest);
        result.candidate_bundle = std::move(candidate.bundle);
        result.value_cell_provenance = std::move(cell_provenance);
        result.parameter_context_provenance = std::move(context_provenance);
    } catch (const std::exception& error) {
        result.diagnostics.push_back(error.what());
    }
    return result;
}
} // namespace datasim::pdf
