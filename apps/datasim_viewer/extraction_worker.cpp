#include "extraction_worker.hpp"
#include <QMetaType>

namespace datasim_viewer {
ExtractionWorker::ExtractionWorker(datasim::pdf::ExtractionRequest request)
    : request_(std::move(request)) {
    qRegisterMetaType<std::shared_ptr<datasim::pdf::ExtractionResult>>(
        "std::shared_ptr<datasim::pdf::ExtractionResult>");
}

void ExtractionWorker::run() {
    auto result = std::make_shared<datasim::pdf::ExtractionResult>();
    try { *result = datasim::pdf::extract_electrical_table(request_); }
    catch (const std::exception& error) { result->diagnostics.push_back(error.what()); }
    emit completed(std::move(result));
}
}
