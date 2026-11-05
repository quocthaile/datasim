#include "../pdf_context_cli.hpp"
#include "datasim/pdf/table_extraction.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
std::vector<std::string> get_utf8_args(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == nullptr) throw std::runtime_error("CommandLineToArgvW failed");
    std::vector<std::string> result;
    result.reserve(static_cast<std::size_t>(wargc));
    for (int i = 0; i < wargc; ++i) {
        const std::wstring_view warg(wargv[i]);
        if (warg.empty()) { result.emplace_back(); continue; }
        const int size = WideCharToMultiByte(CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) { LocalFree(wargv); throw std::runtime_error("WideCharToMultiByte failed"); }
        std::string utf8(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), utf8.data(), size, nullptr, nullptr);
        result.push_back(std::move(utf8));
    }
    LocalFree(wargv);
    return result;
#else
    std::vector<std::string> result;
    for (int i = 0; i < argc; ++i) result.emplace_back(argv[i]);
    return result;
#endif
}

struct ParsedCliArgs {
    std::map<std::string, std::string> values;
    std::vector<std::string> header_unit_bindings;
    std::vector<std::string> inline_unit_bindings;
    std::vector<std::string> parent_context_bindings;
    std::vector<std::string> baseline_text_bindings;
};

ParsedCliArgs parse_args(int argc, char** argv) {
    const auto raw = get_utf8_args(argc, argv);
    ParsedCliArgs parsed;
    for (std::size_t i = 1; i < raw.size(); ++i) {
        const std::string& key = raw[i];
        if (key == "--help") return {{{"--help", "true"}}, {}, {}};
        if (key.rfind("--", 0) != 0 || i + 1 >= raw.size())
            throw std::invalid_argument("expected --option value pairs");
        const std::string& val = raw[++i];
        if (key == "--header-unit-binding") {
            parsed.header_unit_bindings.push_back(val);
            continue;
        }
        if (key == "--baseline-text") {
            parsed.baseline_text_bindings.push_back(val);
            continue;
        }
        if (key == "--parent-context") {
            parsed.parent_context_bindings.push_back(val);
            continue;
        }
        if (key == "--inline-unit-binding") {
            parsed.inline_unit_bindings.push_back(val);
            continue;
        }
        if (!parsed.values.emplace(key, val).second) throw std::invalid_argument("duplicate option: " + key);
    }
    return parsed;
}

std::vector<std::string> split_csv_strings(const std::string& text) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto comma = text.find(',', start);
        result.push_back(text.substr(start, comma == std::string::npos ? comma : comma - start));
        if (comma == std::string::npos) break;
        start = comma + 1U;
    }
    return result;
}

std::vector<std::string> split_pipe_fields(const std::string& spec, std::size_t count) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i + 1U < count; ++i) {
        const auto bar = spec.find('|', start);
        if (bar == std::string::npos) throw std::invalid_argument("invalid pipe-delimited binding specification: " + spec);
        parts.push_back(spec.substr(start, bar - start));
        start = bar + 1U;
    }
    parts.push_back(spec.substr(start));
    return parts;
}

std::vector<double> parse_csv_4(const std::string& text) {
    std::vector<double> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(',', start);
        result.push_back(std::stod(text.substr(start, end - start)));
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    if (result.size() != 4U) throw std::invalid_argument("expected 4 comma-separated coordinates");
    return result;
}

datasim::component::ValueQualifier parse_qualifier(const std::string& text) {
    if (text == "minimum" || text == "min") return datasim::component::ValueQualifier::minimum;
    if (text == "typical" || text == "typ") return datasim::component::ValueQualifier::typical;
    if (text == "maximum" || text == "max") return datasim::component::ValueQualifier::maximum;
    throw std::invalid_argument("invalid header unit binding qualifier: " + text);
}

std::string required(const std::map<std::string, std::string>& args, const char* key) {
    const auto found = args.find(key);
    if (found == args.end() || found->second.empty()) throw std::invalid_argument(std::string("missing required option ") + key);
    return found->second;
}

datasim::pdf::HorizontalSpan span(const std::map<std::string, std::string>& args, const char* key) {
    const auto text = required(args, key);
    const auto comma = text.find(',');
    if (comma == std::string::npos) throw std::invalid_argument(std::string(key) + " must be x_min,x_max");
    return {std::stod(text.substr(0, comma)), std::stod(text.substr(comma + 1U))};
}

std::vector<double> rectangle(const std::map<std::string, std::string>& args) {
    const auto text = required(args, "--region");
    std::vector<double> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto end = text.find(',', start);
        result.push_back(std::stod(text.substr(start, end - start)));
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    if (result.size() != 4U) throw std::invalid_argument("--region must be x_min,y_min,x_max,y_max in PDF points");
    return result;
}

std::filesystem::path find_tool(const char* name) {
    const char* value = std::getenv("PATH");
    if (value == nullptr) return {};
#ifdef _WIN32
    constexpr char separator = ';';
    const std::string suffix = ".exe";
#else
    constexpr char separator = ':';
    const std::string suffix;
#endif
    const std::string paths(value);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(separator, start);
        auto candidate = std::filesystem::path(paths.substr(start, end - start)) / (std::string(name) + suffix);
        if (std::filesystem::exists(candidate)) return candidate;
        if (end == std::string::npos) break;
        start = end + 1U;
    }
    return {};
}

void usage() {
    std::cout << "Usage: datasheet_extract --pdf PATH --page INDEX --printed-page LABEL --table-id ID\n"
                 "  --manufacturer NAME --publisher NAME --mpn EXACT_MPN --family resistor|capacitor\n"
                 "  --component-id ID --document-id ID --title TEXT --revision TEXT\n"
                 "  --rights-note TEXT --redistribution-permitted true|false\n"
                 "  --region x_min,y_min,x_max,y_max --header y_min,y_max\n"
                 "  --parameter x_min,x_max --unit x_min,x_max --min x_min,x_max\n"
                 "  --typ x_min,x_max --max x_min,x_max [--condition x_min,x_max]\n"
                 "  [--header-unit-binding QUALIFIER|x_min,y_min,x_max,y_max|UNIT|EXPECTED_TEXT]\n"
                 "  [--header-condition-binding x_min,y_min,x_max,y_max|PARAM|UNIT|EXPECTED_TEXT]\n"
                 "  [--baseline-text HOST_BOX|HOST|sub/super|SCRIPT_BOX|SCRIPT...]\n"
                 "  [--parent-context BOX|PARENT|ROW:CHILD[|ROW:CHILD...]]\n"
                 "  [--inline-unit-binding QUALIFIER|UNIT1,UNIT2,...]\n"
                 "Coordinates are points in Poppler's page coordinate space. The result is candidate-only.\n";
}
}

int main(int argc, char** argv) {
    try {
        const auto parsed = parse_args(argc, argv);
        const auto& args = parsed.values;
        if (args.contains("--help")) { usage(); return 0; }
        using namespace datasim;
        pdf::ExtractionRequest request;
        request.pdf_path = required(args, "--pdf");
        request.pdfinfo_path = find_tool("pdfinfo");
        request.pdftotext_path = find_tool("pdftotext");
        if (request.pdfinfo_path.empty() || request.pdftotext_path.empty())
            throw std::runtime_error("pdfinfo and pdftotext must be available on PATH");
        request.pdf_page_index = std::stoull(required(args, "--page"));
        request.printed_page_label = required(args, "--printed-page");
        const auto manufacturer = required(args, "--manufacturer");
        const auto mpn = required(args, "--mpn");
        const auto family = required(args, "--family");
        if (family != "resistor" && family != "capacitor")
            throw std::invalid_argument("--family must be resistor or capacitor");
        request.component = {{required(args, "--component-id")}, manufacturer, mpn,
                             std::nullopt, std::nullopt,
                             family == "resistor" ? component::ComponentFamily::resistor : component::ComponentFamily::capacitor,
                             std::nullopt, false};
        const auto document_id = required(args, "--document-id");
        const auto publisher = required(args, "--publisher");
        component::DocumentUsageRights rights;
        rights.redistribution_permitted = required(args, "--redistribution-permitted") == "true";
        rights.restriction_notes = required(args, "--rights-note");
        if (args.contains("--license")) rights.license_identifier = args.at("--license");
        request.source_document = {{document_id}, publisher, required(args, "--title"), {mpn},
                                   required(args, "--revision"), std::nullopt, std::nullopt,
                                   std::nullopt, rights, false};
        if (args.contains("--source-url")) request.source_document.original_url = args.at("--source-url");
        request.curator = {args.contains("--curator") ? args.at("--curator") : "local-curator",
                           component::ActorKind::human_engineer, std::nullopt};
        auto& table = request.table;
        table.table_id = required(args, "--table-id");
        const auto region = rectangle(args);
        table.region_x_min = region[0]; table.region_y_min = region[1];
        table.region_x_max = region[2]; table.region_y_max = region[3];
        const auto header = span(args, "--header");
        table.header_y_min = header.min; table.header_y_max = header.max;
        table.parameter_column = span(args, "--parameter");
        table.unit_column = span(args, "--unit");
        table.minimum_column = span(args, "--min");
        table.typical_column = span(args, "--typ");
        table.maximum_column = span(args, "--max");
        if (args.contains("--condition")) table.condition_column = span(args, "--condition");
        table.row_y_tolerance = args.contains("--row-tolerance") ? std::stod(args.at("--row-tolerance")) : 2.0;
        const auto& category = required(args, "--category");
        if (category == "absolute-maximum") table.category = component::ParameterCategory::absolute_maximum_rating;
        else if (category == "recommended") table.category = component::ParameterCategory::recommended_operating_condition;
        else if (category == "thermal") table.category = component::ParameterCategory::thermal_characteristic;
        else if (category == "other") table.category = component::ParameterCategory::other_unspecified;
        else if (category == "electrical") table.category = component::ParameterCategory::electrical_characteristic;
        else throw std::invalid_argument("unsupported --category value");
        for (const auto& ub_spec : parsed.header_unit_bindings) {
            const auto parts = split_pipe_fields(ub_spec, 4);
            const auto box = parse_csv_4(parts[1]);
            table.header_unit_bindings.push_back(pdf::HeaderUnitBinding{
                parse_qualifier(parts[0]), {box[0], box[2]}, box[1], box[3], parts[3], parts[2]});
        }
        if (args.contains("--header-condition-binding")) {
            const auto parts = split_pipe_fields(args.at("--header-condition-binding"), 4);
            const auto box = parse_csv_4(parts[0]);
            table.header_condition_binding = pdf::HeaderConditionBinding{
                {box[0], box[2]}, box[1], box[3], parts[3], parts[1], parts[2]};
        }
        for (const auto& ib_spec : parsed.inline_unit_bindings) {
            const auto parts = split_pipe_fields(ib_spec, 2);
            table.inline_value_unit_bindings.push_back(pdf::InlineValueUnitBinding{
                parse_qualifier(parts[0]), split_csv_strings(parts[1])});
        }

        for (const auto& spec : parsed.parent_context_bindings)
            table.parent_context_bindings.push_back(pdf::cli::parse_parent_context(spec));

        for (const auto& spec : parsed.baseline_text_bindings)
            table.baseline_text_bindings.push_back(pdf::cli::parse_baseline_text(spec));

        const auto result = pdf::extract_electrical_table(request);
        for (const auto& diagnostic : result.diagnostics) std::cerr << "Extraction diagnostic: " << diagnostic << '\n';
        for (const auto& diagnostic : result.report.diagnostics)
            std::cerr << "Intake diagnostic: " << diagnostic.entity_path << ": " << diagnostic.message << '\n';
        for (const auto& issue : result.report.domain_validation.issues)
            std::cerr << "Domain validation: " << issue.entity_path << ": " << issue.message << '\n';
        if (!result.candidate_bundle.has_value()) {
            std::cerr << "No candidate bundle was created; review source/configuration diagnostics above.\n";
            return 2;
        }
        std::cout << "PDF page count: " << *result.manifest->declared_pdf_page_count
                  << "; selected index: " << request.pdf_page_index
                  << "; printed label (manual): " << *request.printed_page_label << '\n';
        std::cout << "Candidate observations: " << result.candidate_bundle->observations.size()
                  << "; review state: candidate; approved: no\n";
        for (const auto& observation : result.candidate_bundle->observations) {
            std::cout << observation.parameter_name << " | "
                      << observation.original_value_text.value_or("<absent>") << ' '
                      << observation.original_unit_symbol.value_or("") << " | source refs: "
                      << observation.evidence_refs.size() << " | review: candidate\n";
        }
        if (!result.report.valid()) std::cerr << "Intake validation reported errors; bundle construction should have failed closed.\n";
        return 0;
    } catch (const std::exception& error) {
        usage();
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
