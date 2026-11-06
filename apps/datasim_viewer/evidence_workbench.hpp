#pragma once

#include <QWidget>
#include <memory>
#include "review_session.hpp"

namespace datasim::pdf { struct ExtractionResult; }

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QThread;
class QPdfDocument;
class QPdfView;
class QComboBox;
class QPlainTextEdit;
class QCloseEvent;

namespace datasim_viewer {
class EvidenceWorkbench final : public QWidget {
    Q_OBJECT
public:
    explicit EvidenceWorkbench(QWidget* parent = nullptr);
    ~EvidenceWorkbench() override;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void open_pdf();
    void extract();
    void invalidate_results();
    void show_selected_observation();
    void apply_selected_decision();
    QPdfDocument* document_ = nullptr;
    QPdfView* pdf_view_ = nullptr;
    QLabel* document_label_ = nullptr;
    QLabel* status_ = nullptr;
    QTableWidget* observations_ = nullptr;
    QLabel* details_ = nullptr;
    QLabel* review_state_ = nullptr;
    QLabel* history_ = nullptr;
    QLineEdit* reviewer_id_ = nullptr;
    QPlainTextEdit* rationale_ = nullptr;
    QComboBox* evidence_choice_ = nullptr;
    QComboBox* target_state_ = nullptr;
    QLineEdit* replacement_id_ = nullptr;
    std::unique_ptr<ReviewSession> review_session_;
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
