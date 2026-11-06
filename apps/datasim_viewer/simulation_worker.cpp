#include "simulation_worker.hpp"

#include <QFileInfo>
#include <QString>
#include <exception>

namespace datasim_viewer {
namespace {
QString path_string(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}

QStringList existing_artifacts(const std::filesystem::path& directory) {
    QStringList found;
    for (const char* name : {"results.csv", "errors.csv", "circuit.cir", "run_metadata.json",
                             "validation_report.json", "ngspice.log", "version.txt", "samples.dat"}) {
        const auto path = directory / name;
        if (QFileInfo::exists(path_string(path))) found.push_back(QString::fromLatin1(name));
    }
    return found;
}
}

SimulationWorker::SimulationWorker(datasim::RcConfig config, std::string executable,
                                   std::filesystem::path output_directory)
    : config_(config), executable_(std::move(executable)), output_directory_(std::move(output_directory)) {}

void SimulationWorker::execute() {
    try {
        auto result = datasim::execute_rc_run(config_, executable_, output_directory_);
        if (!result.validation.passed)
            throw std::runtime_error("datasim returned a run without passing numerical validation");
        emit succeeded(std::move(result), path_string(output_directory_), existing_artifacts(output_directory_));
    } catch (const std::exception& error) {
        emit failed(QString::fromUtf8(error.what()), path_string(output_directory_),
                    existing_artifacts(output_directory_));
    } catch (...) {
        emit failed(QStringLiteral("Unknown failure during ideal-RC run"), path_string(output_directory_),
                    existing_artifacts(output_directory_));
    }
    emit finished();
}

} // namespace datasim_viewer
