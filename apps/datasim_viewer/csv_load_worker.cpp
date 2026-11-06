#include "csv_load_worker.hpp"
#include "read_only_samples.hpp"

namespace datasim_viewer {
CsvLoadWorker::CsvLoadWorker(std::filesystem::path path) : path_(std::move(path)) {}

void CsvLoadWorker::execute() {
    try {
        auto loaded_samples = load_samples_csv(path_);
        const auto encoded = path_.u8string();
        emit loaded(std::move(loaded_samples.samples),
                    QString::fromUtf8(reinterpret_cast<const char*>(encoded.data()),
                                      static_cast<qsizetype>(encoded.size())));
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()));
    }
    emit finished();
}
}
