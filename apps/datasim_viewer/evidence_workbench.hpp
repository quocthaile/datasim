#pragma once

#include <QWidget>
#include <memory>

namespace datasim::pdf { struct ExtractionResult; }

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QThread;
class QPdfDocument;
class QPdfView;

namespace datasim_viewer {
class EvidenceWorkbench final : public QWidget {
    Q_OBJECT
public:
    explicit EvidenceWorkbench(QWidget* parent = nullptr);
    ~EvidenceWorkbench() override;
private:
    void open_pdf();
    void extract();
    void invalidate_results();
    QPdfDocument* document_ = nullptr;
    QPdfView* pdf_view_ = nullptr;
    QLabel* document_label_ = nullptr;
    QLabel* status_ = nullptr;
    QTableWidget* observations_ = nullptr;
    QLabel* details_ = nullptr;
    QLineEdit* pdf_path_ = nullptr;
    QLineEdit* edits_[30]{};
    QComboBox* family_ = nullptr;
    QComboBox* category_ = nullptr;
    QPushButton* extract_button_ = nullptr;
    QThread* thread_ = nullptr;
    QString request_key_;
    std::shared_ptr<datasim::pdf::ExtractionResult> latest_result_;
};
}
