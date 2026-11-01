#pragma once
#include "datasim/component/identity/ids.hpp"
#include <compare>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

// Provenance locator within a source document. PDF zero-based page index is distinct
// from the printed page label and neither is inferred from the other.
struct EvidenceLocation {
    EvidenceId id;
    SourceDocumentId source_document_id;
    std::optional<std::size_t> pdf_page_index;
    std::optional<std::string> printed_page_label;
    std::optional<std::string> section;
    std::optional<std::string> table_or_figure_id;
    std::optional<std::string> cell_or_region;
    std::vector<std::string> footnote_refs;

    auto operator<=>(const EvidenceLocation&) const = default;
};

} // namespace datasim::component
