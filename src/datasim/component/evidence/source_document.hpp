#pragma once
#include "datasim/component/identity/ids.hpp"
#include <compare>
#include <optional>
#include <string>
#include <vector>

namespace datasim::component {

struct DocumentUsageRights {
    std::optional<std::string> license_identifier;
    bool redistribution_permitted = false;
    std::string restriction_notes;

    auto operator<=>(const DocumentUsageRights&) const = default;
};

// Metadata describing a source document; never embeds copyrighted vendor PDFs.
struct SourceDocument {
    SourceDocumentId id;
    std::string publisher;
    std::string title;
    std::vector<std::string> associated_part_numbers;
    std::string revision;
    std::optional<std::string> original_url;
    std::optional<std::string> retrieval_date;
    std::optional<std::string> content_sha256;
    DocumentUsageRights usage_rights;
    bool is_synthetic = false;

    auto operator<=>(const SourceDocument&) const = default;
};

} // namespace datasim::component
