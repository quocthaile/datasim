#pragma once
#include <string>

namespace datasim::pdf::detail {
// Internal regression seam for validating the exact Poppler bbox word parser.
void validate_poppler_bbox_markup(const std::string& markup);
}
