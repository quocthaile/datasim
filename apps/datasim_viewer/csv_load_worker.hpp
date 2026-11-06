#pragma once

#include "datasim/simulation/io/samples.hpp"
#include <QObject>
#include <QString>
#include <filesystem>

Q_DECLARE_METATYPE(datasim::Samples)

namespace datasim_viewer {
class CsvLoadWorker final : public QObject {
    Q_OBJECT
public:
    explicit CsvLoadWorker(std::filesystem::path path);
public slots:
    void execute();
signals:
    void loaded(datasim::Samples samples, QString source_path);
    void failed(QString message);
    void finished();
private:
    std::filesystem::path path_;
};
}
