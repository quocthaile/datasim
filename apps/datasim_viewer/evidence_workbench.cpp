#include "evidence_workbench.hpp"
#include "extraction_worker.hpp"

#include "datasim/pdf/table_extraction.hpp"
#include "datasim/component/identity/component_identity.hpp"
#include "datasim/component/identity/ids.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QThread>
#include <QVBoxLayout>
#include <QHeaderView>

#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <memory>

namespace {
using namespace datasim;
using namespace datasim::component;
using namespace datasim::pdf;

double number(const QString& text, const char* label) {
    const auto bytes = text.toLatin1();
    char* end = nullptr;
    const auto value = std::strtod(bytes.constData(), &end);
    if (bytes.isEmpty() || end != bytes.constData() + bytes.size() || !std::isfinite(value))
        throw std::invalid_argument(std::string("Invalid numeric field: ") + label);
    return value;
}

HorizontalSpan span(const QString& a, const QString& b, const char* label) {
    HorizontalSpan result{number(a, label), number(b, label)};
    if (!(result.max > result.min)) throw std::invalid_argument(std::string("Invalid span: ") + label);
    return result;
}

std::filesystem::path find_tool(const char* executable) {
    const char* raw = std::getenv("PATH");
    if (!raw) return {};
#ifdef _WIN32
    constexpr char separator = ';';
    const std::string suffix = ".exe";
#else
    constexpr char separator = ':';
    const std::string suffix;
#endif
    const std::string paths(raw);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(separator, start);
        const auto candidate = std::filesystem::path(paths.substr(start, end - start)) / (std::string(executable) + suffix);
        if (std::filesystem::is_regular_file(candidate)) return candidate;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

QString review_state(ReviewState state) {
    switch (state) {
    case ReviewState::candidate: return "candidate";
    case ReviewState::needs_review: return "needs_review";
    case ReviewState::approved: return "approved";
    case ReviewState::rejected: return "rejected";
    case ReviewState::superseded: return "superseded";
    }
    return "unknown";
}

QString qualifier(ValueQualifier value) {
    switch (value) {
    case ValueQualifier::minimum: return "minimum";
    case ValueQualifier::typical: return "typical";
    case ValueQualifier::maximum: return "maximum";
    case ValueQualifier::unspecified: return "unspecified";
    }
    return "unspecified";
}

QString category_name(ParameterCategory value) {
    switch (value) {
    case ParameterCategory::absolute_maximum_rating: return "absolute_maximum_rating";
    case ParameterCategory::recommended_operating_condition: return "recommended_operating_condition";
    case ParameterCategory::electrical_characteristic: return "electrical_characteristic";
    case ParameterCategory::thermal_characteristic: return "thermal_characteristic";
    case ParameterCategory::other_unspecified: return "other_unspecified";
    }
    return "other_unspecified";
}

}

namespace datasim_viewer {
EvidenceWorkbench::EvidenceWorkbench(QWidget* parent) : QWidget(parent), document_(new QPdfDocument(this)) {
    auto* root = new QHBoxLayout(this);
    auto* config_scroll = new QScrollArea(this);
    config_scroll->setWidgetResizable(true);
    config_scroll->setMinimumWidth(370);
    auto* config = new QWidget(config_scroll);
    auto* column = new QVBoxLayout(config);
    auto* open_row = new QHBoxLayout;
    pdf_path_ = new QLineEdit(config);
    pdf_path_->setReadOnly(true);
    auto* open_button = new QPushButton("Open PDF…", config);
    open_row->addWidget(pdf_path_, 1);
    open_row->addWidget(open_button);
    column->addLayout(open_row);
    document_label_ = new QLabel("No PDF loaded", config);
    column->addWidget(document_label_);

    const char* labels[30] = {
        "Component identity", "Manufacturer designation", "Exact MPN", "Source document identity", "Publisher", "Document title", "Revision", "Usage rights notes (no permission asserted)",
        "PDF page index (zero-based)", "Printed page label (manual)", "Table identity", "Region x min (pt)", "Region y min (pt)", "Region x max (pt)", "Region y max (pt)", "Header y min (pt)", "Header y max (pt)",
        "Parameter x min (pt)", "Parameter x max (pt)", "Unit x min (pt)", "Unit x max (pt)", "Minimum x min (pt)", "Minimum x max (pt)", "Typical x min (pt)", "Typical x max (pt)", "Maximum x min (pt)", "Maximum x max (pt)", "Row Y tolerance (pt)", "Condition x min (optional)", "Condition x max (optional)"};
    auto* identity = new QGroupBox("Explicit extraction configuration", config);
    auto* form = new QFormLayout(identity);
    for (int i = 0; i < 30; ++i) {
        edits_[i] = new QLineEdit(identity);
        edits_[i]->setObjectName(QStringLiteral("evidenceField%1").arg(i));
        form->addRow(QString::fromLatin1(labels[i]), edits_[i]);
        connect(edits_[i], &QLineEdit::textChanged, this, [this] { invalidate_results(); });
    }
    family_ = new QComboBox(identity);
    family_->addItem("Select component family", -1);
    family_->addItem("Resistor", static_cast<int>(ComponentFamily::resistor));
    family_->addItem("Capacitor", static_cast<int>(ComponentFamily::capacitor));
    family_->addItem("Inductor", static_cast<int>(ComponentFamily::inductor));
    family_->addItem("Other", static_cast<int>(ComponentFamily::other));
    category_ = new QComboBox(identity);
    category_->addItem("Select parameter category", -1);
    category_->addItem("Absolute maximum rating", static_cast<int>(ParameterCategory::absolute_maximum_rating));
    category_->addItem("Recommended operating condition", static_cast<int>(ParameterCategory::recommended_operating_condition));
    category_->addItem("Electrical characteristic", static_cast<int>(ParameterCategory::electrical_characteristic));
    category_->addItem("Thermal characteristic", static_cast<int>(ParameterCategory::thermal_characteristic));
    category_->addItem("Other unspecified", static_cast<int>(ParameterCategory::other_unspecified));
    form->insertRow(3, "Component family", family_);
    form->insertRow(form->rowCount(), "Parameter category", category_);
    connect(family_, &QComboBox::currentIndexChanged, this, [this] { invalidate_results(); });
    connect(category_, &QComboBox::currentIndexChanged, this, [this] { invalidate_results(); });
    column->addWidget(identity);
    auto* coordinates = new QLabel("Authoritative coordinates: Poppler page coordinates, top-origin, points. Numeric entry only; mouse selection deferred. Qt PDF is display-only.", config);
    coordinates->setWordWrap(true);
    column->addWidget(coordinates);
    extract_button_ = new QPushButton("Extract candidate evidence", config);
    column->addWidget(extract_button_);
    status_ = new QLabel("Idle: configure document and explicit table coordinates", config);
    status_->setWordWrap(true);
    column->addWidget(status_);
    column->addStretch();
    config_scroll->setWidget(config);

    auto* document_column = new QVBoxLayout;
    auto* navigation = new QHBoxLayout;
    auto* previous = new QPushButton("Previous page", this);
    auto* next = new QPushButton("Next page", this);
    auto* fit_width = new QPushButton("Fit width", this);
    auto* fit_page = new QPushButton("Fit page", this);
    auto* zoom_out = new QPushButton("−", this);
    auto* zoom_in = new QPushButton("+", this);
    auto* page_info = new QLabel("Page —", this);
    navigation->addWidget(previous); navigation->addWidget(next); navigation->addWidget(page_info);
    navigation->addStretch(); navigation->addWidget(zoom_out); navigation->addWidget(zoom_in); navigation->addWidget(fit_width); navigation->addWidget(fit_page);
    document_column->addLayout(navigation);
    pdf_view_ = new QPdfView(this);
    pdf_view_->setDocument(document_);
    pdf_view_->setPageMode(QPdfView::PageMode::SinglePage);
    pdf_view_->setZoomMode(QPdfView::ZoomMode::FitInView);
    document_column->addWidget(pdf_view_, 3);
    observations_ = new QTableWidget(0, 8, this);
    observations_->setHorizontalHeaderLabels({"Parameter", "Original value", "Unit", "Parsed numeric", "Qualifier", "Category", "Conditions", "Review state"});
    observations_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    document_column->addWidget(observations_, 2);
    details_ = new QLabel("Select a candidate row to inspect its source evidence and extraction provenance.", this);
    details_->setWordWrap(true);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    document_column->addWidget(details_);
    root->addWidget(config_scroll, 0);
    auto* right = new QWidget(this);
    right->setLayout(document_column);
    root->addWidget(right, 1);

    connect(open_button, &QPushButton::clicked, this, [this] { open_pdf(); });
    connect(extract_button_, &QPushButton::clicked, this, [this] { extract(); });
    connect(observations_, &QTableWidget::currentCellChanged, this, [this](int row, int, int, int) {
        if (!latest_result_ || !latest_result_->candidate_bundle || row < 0 || row >= static_cast<int>(latest_result_->candidate_bundle->observations.size())) return;
        const auto& observation = latest_result_->candidate_bundle->observations[static_cast<std::size_t>(row)];
        QStringList evidence;
        for (const auto& id : observation.evidence_refs) evidence << QString::fromStdString(id.value);
        QStringList lines;
        lines << QString("Observation ID: %1").arg(QString::fromStdString(observation.id.value));
        lines << QString("SourceDocument ID: %1").arg(edits_[3]->text());
        lines << QString("PDF page index: %1; printed page label: %2").arg(edits_[8]->text(), edits_[9]->text());
        lines << QString("Table/figure identifier: %1; evidence IDs: %2").arg(edits_[10]->text(), evidence.join(", "));
        for (const auto& location : latest_result_->candidate_bundle->evidence_locations) {
            if (std::find(observation.evidence_refs.begin(), observation.evidence_refs.end(), location.id) == observation.evidence_refs.end()) continue;
            lines << QString("EvidenceLocation: id=%1 SourceDocument ID=%2 PDF page index=%3 printed page label=%4 table/figure=%5 cell/region=%6")
                .arg(QString::fromStdString(location.id.value), QString::fromStdString(location.source_document_id.value))
                .arg(location.pdf_page_index ? QString::number(*location.pdf_page_index) : "<absent>",
                     location.printed_page_label ? QString::fromStdString(*location.printed_page_label) : "<absent>")
                .arg(location.table_or_figure_id ? QString::fromStdString(*location.table_or_figure_id) : "<absent>",
                     location.cell_or_region ? QString::fromStdString(*location.cell_or_region) : "<absent>");
        }
        lines << QString("Original value text: %1; original unit symbol: %2").arg(QString::fromStdString(observation.original_value_text.value_or("<absent>")), QString::fromStdString(observation.original_unit_symbol.value_or("<absent>")));
        lines << QString("Category: %1; review state: %2").arg(category_name(observation.category), review_state(observation.review_state));
        for (const auto& condition : observation.operating_conditions)
            lines << QString("Condition %1=%2 %3").arg(QString::fromStdString(condition.parameter_name), QString::fromStdString(condition.original_value_text), QString::fromStdString(condition.unit_symbol));
        for (const auto& provenance : latest_result_->value_cell_provenance) if (provenance.observation_id == observation.id)
            lines << QString("ValueCellProvenance: row=%1 qualifier=%2 raw_cell_text=%3 raw_value_token=%4 inline_unit=%5")
                .arg(provenance.row_index).arg(qualifier(provenance.qualifier), QString::fromStdString(provenance.raw_cell_text), QString::fromStdString(provenance.raw_value_token), QString::fromStdString(provenance.raw_inline_unit_token.value_or("<absent>")));
        for (const auto& provenance : latest_result_->parameter_context_provenance) if (provenance.observation_id == observation.id)
            lines << QString("ParameterContextProvenance: parent evidence=%1 child evidence=%2 row=%3 parent=%4 child=%5")
                .arg(QString::fromStdString(provenance.parent_evidence_id.value), QString::fromStdString(provenance.child_evidence_id.value)).arg(provenance.row_index)
                .arg(QString::fromStdString(provenance.raw_parent_text), QString::fromStdString(provenance.raw_child_parameter_text));
        for (const auto& provenance : latest_result_->baseline_text_provenance) if (provenance.observation_id == observation.id)
            lines << QString("BaselineTextProvenance: raw host=%1 source=%2 display=%3")
                .arg(QString::fromStdString(provenance.raw_host_text), QString::fromStdString(provenance.source_text), QString::fromStdString(provenance.display_expression));
        details_->setText(lines.join("\n"));
    });
    connect(previous, &QPushButton::clicked, this, [this] { auto* n = pdf_view_->pageNavigator(); n->jump(std::max(0, n->currentPage() - 1), {}); });
    connect(next, &QPushButton::clicked, this, [this] { auto* n = pdf_view_->pageNavigator(); n->jump(std::min(document_->pageCount() - 1, n->currentPage() + 1), {}); });
    connect(fit_width, &QPushButton::clicked, this, [this] { pdf_view_->setZoomMode(QPdfView::ZoomMode::FitToWidth); });
    connect(fit_page, &QPushButton::clicked, this, [this] { pdf_view_->setZoomMode(QPdfView::ZoomMode::FitInView); });
    connect(zoom_in, &QPushButton::clicked, this, [this] { pdf_view_->setZoomMode(QPdfView::ZoomMode::Custom); pdf_view_->setZoomFactor(pdf_view_->zoomFactor() * 1.2); });
    connect(zoom_out, &QPushButton::clicked, this, [this] { pdf_view_->setZoomMode(QPdfView::ZoomMode::Custom); pdf_view_->setZoomFactor(pdf_view_->zoomFactor() / 1.2); });
    connect(document_, &QPdfDocument::pageCountChanged, this, [this, page_info](int count) {
        connect(pdf_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, page_info, [this, page_info, count](int page) {
            page_info->setText(QString("Page %1 / %2 (zero-based index %3)").arg(page + 1).arg(count).arg(page));
        });
        page_info->setText(count ? QString("Page 1 / %1 (zero-based index 0)").arg(count) : "Page —");
    });
    connect(pdf_view_->pageNavigator(), &QPdfPageNavigator::currentPageChanged, this, [this](int page) {
        if (page >= 0) edits_[8]->setText(QString::number(page));
    });
}

EvidenceWorkbench::~EvidenceWorkbench() {
    if (thread_) { thread_->quit(); thread_->wait(); }
}

void EvidenceWorkbench::open_pdf() {
    const auto path = QFileDialog::getOpenFileName(this, "Open local PDF (read-only)", {}, "PDF files (*.pdf)");
    if (path.isEmpty()) return;
    invalidate_results();
    document_->close();
    const auto error = document_->load(path);
    if (error != QPdfDocument::Error::None || document_->status() != QPdfDocument::Status::Ready) {
        document_label_->setText(QString("PDF loading failed: Qt error %1").arg(static_cast<int>(error)));
        status_->setText("Failed: PDF could not be opened or rendered");
        pdf_path_->clear();
        return;
    }
    pdf_path_->setText(path);
    document_label_->setText(QString("%1 pages; current page is zero-based. Source opened read-only.").arg(document_->pageCount()));
    edits_[8]->setText("0");
    status_->setText("Configuring: document loaded; page count shown above");
}

void EvidenceWorkbench::invalidate_results() {
    observations_->setRowCount(0);
    latest_result_.reset();
    details_->setText("Candidate results invalidated after document/configuration change.");
    request_key_.clear();
    if (!thread_) status_->setText(pdf_path_->text().isEmpty() ? "Idle: configure document and explicit table coordinates" : "Configuring: evidence candidates invalidated after document/configuration change");
}

void EvidenceWorkbench::extract() {
    if (thread_) return;
    try {
        if (pdf_path_->text().isEmpty() || document_->status() != QPdfDocument::Status::Ready) throw std::invalid_argument("Open a valid local PDF first");
        ExtractionRequest request;
        request.pdf_path = pdf_path_->text().toStdString();
        request.pdfinfo_path = find_tool("pdfinfo");
        request.pdftotext_path = find_tool("pdftotext");
        if (request.pdfinfo_path.empty() || request.pdftotext_path.empty()) throw std::invalid_argument("Poppler pdfinfo and pdftotext executables must be available on PATH");
        const double page_index = number(edits_[8]->text(), "PDF page index");
        if (page_index < 0 || std::floor(page_index) != page_index) throw std::invalid_argument("PDF page index must be a non-negative integer");
        request.pdf_page_index = static_cast<std::size_t>(page_index);
        if (request.pdf_page_index >= static_cast<std::size_t>(document_->pageCount())) throw std::invalid_argument("PDF page index is outside the loaded document");
        if (!edits_[9]->text().isEmpty()) request.printed_page_label = edits_[9]->text().toStdString();
        const auto required = [this](int i, const char* name) {
            const auto value = edits_[i]->text().trimmed();
            if (value.isEmpty()) throw std::invalid_argument(std::string("Required field is empty: ") + name);
            return value.toStdString();
        };
        if (family_->currentData().toInt() < 0) throw std::invalid_argument("Select a component family");
        if (category_->currentData().toInt() < 0) throw std::invalid_argument("Select a parameter category");
        request.component = {{required(0, "component identity")}, required(1, "manufacturer"), required(2, "exact MPN"), std::nullopt, std::nullopt, static_cast<ComponentFamily>(family_->currentData().toInt()), std::nullopt, false};
        DocumentUsageRights rights; rights.restriction_notes = edits_[7]->text().toStdString();
        request.source_document = {{required(3, "source document identity")}, required(4, "publisher"), required(5, "document title"), {required(2, "exact MPN")}, required(6, "revision"), std::nullopt, std::nullopt, std::nullopt, rights, false};
        request.curator = {"local-curator", ActorKind::human_engineer, std::nullopt};
        auto& table = request.table;
        table.table_id = required(10, "table identity");
        table.region_x_min = number(edits_[11]->text(), "region x min"); table.region_y_min = number(edits_[12]->text(), "region y min");
        table.region_x_max = number(edits_[13]->text(), "region x max"); table.region_y_max = number(edits_[14]->text(), "region y max");
        table.header_y_min = number(edits_[15]->text(), "header y min"); table.header_y_max = number(edits_[16]->text(), "header y max");
        table.parameter_column = span(edits_[17]->text(), edits_[18]->text(), "parameter column");
        table.unit_column = span(edits_[19]->text(), edits_[20]->text(), "unit column");
        table.minimum_column = span(edits_[21]->text(), edits_[22]->text(), "minimum column");
        table.typical_column = span(edits_[23]->text(), edits_[24]->text(), "typical column");
        table.maximum_column = span(edits_[25]->text(), edits_[26]->text(), "maximum column");
        table.row_y_tolerance = number(edits_[27]->text().isEmpty() ? "2" : edits_[27]->text(), "row Y tolerance");
        if (!edits_[28]->text().isEmpty() || !edits_[29]->text().isEmpty()) table.condition_column = span(edits_[28]->text(), edits_[29]->text(), "condition column");
        table.category = static_cast<ParameterCategory>(category_->currentData().toInt());
        if (!(table.region_x_max > table.region_x_min && table.region_y_max > table.region_y_min && table.header_y_max > table.header_y_min &&
              table.header_y_min >= table.region_y_min && table.header_y_max <= table.region_y_max && table.row_y_tolerance > 0))
            throw std::invalid_argument("Table region/header ranges and row tolerance must be valid and contained");
        std::vector<HorizontalSpan> columns{table.parameter_column, table.unit_column, table.minimum_column, table.typical_column, table.maximum_column};
        if (table.condition_column) columns.push_back(*table.condition_column);
        std::sort(columns.begin(), columns.end(), [](const auto& left, const auto& right) { return left.min < right.min; });
        for (std::size_t i = 0; i < columns.size(); ++i) {
            if (columns[i].min < table.region_x_min || columns[i].max > table.region_x_max)
                throw std::invalid_argument("Every table column span must be within the explicit region");
            if (i != 0 && columns[i].min < columns[i - 1].max)
                throw std::invalid_argument("Table column spans overlap");
        }
        request_key_ = pdf_path_->text() + edits_[0]->text() + edits_[2]->text() + edits_[10]->text() + edits_[11]->text();
        extract_button_->setEnabled(false);
        status_->setText("Extracting: running bounded Poppler extraction off the GUI thread");
        thread_ = new QThread(this);
        auto* task = new ExtractionWorker(std::move(request));
        task->moveToThread(thread_);
        connect(thread_, &QThread::started, task, &ExtractionWorker::run);
        const auto result_key = request_key_;
        connect(task, &ExtractionWorker::completed, this, [this, result_key](std::shared_ptr<ExtractionResult> result) {
            if (!pdf_path_->text().isEmpty() && request_key_ == result_key) {
                latest_result_ = result;
                observations_->setRowCount(0);
                if (result->candidate_bundle) {
                    const auto& list = result->candidate_bundle->observations;
                    observations_->setRowCount(static_cast<int>(list.size()));
                    for (int row = 0; row < static_cast<int>(list.size()); ++row) {
                        const auto& o = list[static_cast<std::size_t>(row)];
                        QStringList conditions;
                        for (const auto& c : o.operating_conditions) conditions << QString("%1=%2 %3").arg(QString::fromStdString(c.parameter_name), QString::fromStdString(c.original_value_text), QString::fromStdString(c.unit_symbol));
                        const QString values[] = {QString::fromStdString(o.parameter_name), QString::fromStdString(o.original_value_text.value_or("<absent>")), QString::fromStdString(o.original_unit_symbol.value_or("<absent>")), o.parsed_numeric_value ? QString::number(*o.parsed_numeric_value, 'g', 17) : "<absent>", qualifier(o.qualifier), category_name(o.category), conditions.join("; "), review_state(o.review_state)};
                        for (int col = 0; col < 8; ++col) observations_->setItem(row, col, new QTableWidgetItem(values[col]));
                    }
                }
                QStringList notes;
                for (const auto& d : result->diagnostics) notes << QString::fromStdString(d);
                for (const auto& d : result->report.diagnostics) notes << QString::fromStdString(d.entity_path + ": " + d.message);
                for (const auto& issue : result->report.domain_validation.issues) notes << QString::fromStdString(issue.entity_path + ": " + issue.message);
                status_->setText(result->candidate_bundle ? QString("CandidatesReady: %1 candidate observations. Review state preserved; automatic approval is disabled.\n%2").arg(result->candidate_bundle->observations.size()).arg(notes.join("\n")) : QString("Failed: no candidate bundle.\n%1").arg(notes.join("\n")));
            } else if (request_key_ != result_key) status_->setText("Configuring: extraction finished for an earlier document/configuration; stale candidates discarded.");
            extract_button_->setEnabled(true);
        }, Qt::QueuedConnection);
        connect(task, &ExtractionWorker::completed, thread_, &QThread::quit);
        connect(thread_, &QThread::finished, task, &QObject::deleteLater);
        connect(thread_, &QThread::finished, this, [this] { thread_->deleteLater(); thread_ = nullptr; extract_button_->setEnabled(true); });
        thread_->start();
    } catch (const std::exception& error) { status_->setText(QString("Configuring: %1").arg(error.what())); }
}
}

