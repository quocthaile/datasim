#include "table_extraction_test.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::string word(const std::string& x0, const std::string& y0 = "2.5",
                 const std::string& x1 = "10.25", const std::string& y1 = "9e0") {
    return "<word xMin=\"" + x0 + "\" yMin=\"" + y0 + "\" xMax=\"" + x1 +
           "\" yMax=\"" + y1 + "\">x</word>";
}
int main() {
    try {
        for (const auto& bad : {"10..2", "1.2.3", "+", "-", ".", "1e999", "NaN", "Infinity"}) {
            bool rejected = false;
            try { datasim::pdf::detail::validate_poppler_bbox_markup(word(bad)); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "malformed, non-finite, or overflowing coordinate was accepted");
        }
        datasim::pdf::detail::validate_poppler_bbox_markup(word("-1.25e1", "-3.5", "1.0e1", "9.0"));
        for (const auto& bad : {word("1", "2", "1", "9"), word("1", "2", "3", "2")}) {
            bool rejected = false;
            try { datasim::pdf::detail::validate_poppler_bbox_markup(bad); }
            catch (const std::exception&) { rejected = true; }
            require(rejected, "zero-area or inverted PDF bbox was accepted");
        }
        std::cout << "PASS: complete finite Poppler coordinates and positive-area geometry\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
