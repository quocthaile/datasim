#pragma once

#include "datasim/simulation/circuit/rc.hpp"
#include "datasim/simulation/io/report.hpp"
#include <QObject>
#include <QStringList>
#include <filesystem>

namespace datasim_viewer {

class SimulationWorker final : public QObject {
    Q_OBJECT
public:
    SimulationWorker(datasim::RcConfig config, std::string executable,
                     std::filesystem::path output_directory);

public slots:
    void execute();

signals:
    void succeeded(datasim::RunResult result, QString output_directory, QStringList artifacts);
    void failed(QString error, QString output_directory, QStringList artifacts);
    void finished();

private:
    datasim::RcConfig config_;
    std::string executable_;
    std::filesystem::path output_directory_;
};

} // namespace datasim_viewer

Q_DECLARE_METATYPE(datasim::RunResult)
