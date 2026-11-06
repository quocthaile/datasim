#pragma once

#include "datasim/pdf/table_extraction.hpp"
#include <QObject>
#include <memory>

namespace datasim_viewer {
class ExtractionWorker final : public QObject {
    Q_OBJECT
public:
    explicit ExtractionWorker(datasim::pdf::ExtractionRequest request);
public slots:
    void run();
signals:
    void completed(std::shared_ptr<datasim::pdf::ExtractionResult> result);
private:
    datasim::pdf::ExtractionRequest request_;
};
}

Q_DECLARE_METATYPE(std::shared_ptr<datasim::pdf::ExtractionResult>)
