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
        for (const auto span : spans)
            if (std::max(span.min, t.condition_column->min) < std::min(span.max, t.condition_column->max))
                throw std::invalid_argument("condition column overlaps another column");
    }
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

struct TempDirectory {
    std::filesystem::path path;
    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
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

        intake::IntakeManifest manifest;
        manifest.origin = request.component.is_synthetic ? intake::IntakeOriginClass::synthetic_fixture
                                                          : intake::IntakeOriginClass::manual_curation;
        manifest.curator = request.curator;
        manifest.component = request.component;
        manifest.source_document = request.source_document;
        manifest.declared_pdf_page_count = page_count;
        const auto version = tool_version(request.pdftotext_path, temp.path);
        std::size_t row_number = 0;
        for (const auto& row : rows) {
            ++row_number;
            std::string parameter = row_cell(row.words, request.table.parameter_column);
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
                const std::string value = row_cell(row.words, column);
                if (value.empty() || value == "-" || value == std::string("\xE2\x80\x94")) continue;
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
                if (value.find('*') != std::string::npos) evidence.footnote_refs = {"*"};
                component::EvidenceLocation parameter_evidence = evidence;
                parameter_evidence.id = {"ev." + base + ".parameter"};
                parameter_evidence.cell_or_region = "row=" + std::to_string(row_number) + ";column=parameter/unit";
                parameter_evidence.footnote_refs.clear();
                manifest.evidence.push_back({parameter_evidence, parameter, "parameter", false});
                manifest.evidence.push_back({evidence, parameter, column_name, false});

                component::ParameterObservation observation;
                observation.id = {"obs." + base};
                observation.component_id = request.component.id;
                observation.parameter_name = parameter;
                observation.original_value_text = value;
                observation.original_unit_symbol = unit.empty() ? std::nullopt : std::optional<std::string>(unit);
                observation.unitless = false;
                observation.parsed_numeric_value = unit.empty() ? std::nullopt : intake::parse_unambiguous_scalar(value);
                observation.qualifier = qualifier;
                observation.category = *request.table.category;
                const auto conditions = parse_conditions(condition_text);
                if (conditions.has_value()) observation.operating_conditions = *conditions;
                observation.evidence_refs = {parameter_evidence.id, evidence.id};
                observation.extraction_method = component::ExtractionMethod::deterministic_table_parser;
                observation.extraction_tool = "Poppler pdftotext -bbox-layout";
                observation.extraction_tool_version = version;
                intake::IntakeObservationEntry entry;
                entry.observation = std::move(observation);
                if (!condition_text.empty()) {
                    entry.raw_conditions_text = condition_text;
                    if (!conditions.has_value()) {
                        entry.ambiguity_notes = "Condition text preserved verbatim but does not match the supported PARAMETER=value UNIT form.";
                        result.diagnostics.push_back("row " + std::to_string(row_number) + ": operating condition left unresolved: " + condition_text);
                    }
                }
                if (unit.empty())
                    result.diagnostics.push_back("row " + std::to_string(row_number) +
                        ": unit cell is blank; no unitless status or numeric value was inferred");
                manifest.observations.push_back(std::move(entry));
            }
        }
        if (manifest.observations.empty()) throw std::runtime_error("selected region contained no supported min/typ/max value cells");
        auto candidate = intake::build_candidate_bundle(manifest);
        result.report = std::move(candidate.report);
        result.manifest = std::move(manifest);
        result.candidate_bundle = std::move(candidate.bundle);
    } catch (const std::exception& error) {
        result.diagnostics.push_back(error.what());
    }
    return result;
}
} // namespace datasim::pdf
