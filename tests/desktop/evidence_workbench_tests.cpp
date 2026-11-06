#include "evidence_workbench.hpp"
#include "extraction_worker.hpp"
#include "datasim/pdf/table_extraction.hpp"

#include <QApplication>
#include <QFile>
#include <QPdfDocument>
#include <QPdfPageNavigator>
#include <QPdfView>
#include <QLineEdit>
#include <QPushButton>
#include <QEventLoop>
#include <QThread>
#include <QTemporaryDir>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <filesystem>

#ifndef DATASIM_SYNTHETIC_PDF_PATH
#error DATASIM_SYNTHETIC_PDF_PATH must be set
#endif
#ifndef DATASIM_IMAGE_ONLY_PDF_PATH
#error DATASIM_IMAGE_ONLY_PDF_PATH must be set
#endif

namespace {
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

std::filesystem::path tool(const char* name) {
    const char* path = std::getenv("PATH");
    if (!path) return {};
#ifdef _WIN32
    constexpr char separator = ';';
    const std::string suffix = ".exe";
#else
    constexpr char separator = ':';
    const std::string suffix;
#endif
    const std::string paths(path);
    std::size_t start = 0;
    while (start <= paths.size()) {
        const auto end = paths.find(separator, start);
        const auto candidate = std::filesystem::path(paths.substr(start, end - start)) / (std::string(name) + suffix);
        if (std::filesystem::is_regular_file(candidate)) return candidate;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return {};
}

datasim::pdf::ExtractionRequest extraction_request() {
    using namespace datasim;
    pdf::ExtractionRequest r;
    r.pdf_path = DATASIM_SYNTHETIC_PDF_PATH;
    r.pdfinfo_path = tool("pdfinfo"); r.pdftotext_path = tool("pdftotext");
    r.printed_page_label = "5";
    r.curator = {"fixture-curator", component::ActorKind::human_engineer, std::nullopt};
    r.component = {{"cmp.ui.test"}, "Datasim Synthetic", "SIM-R-10K", std::nullopt, std::nullopt, component::ComponentFamily::resistor, std::nullopt, true};
    r.source_document = {{"doc.ui.test"}, "Datasim Synthetic", "Fictional electrical characteristics", {"SIM-R-10K"}, "fixture-rev-1", std::nullopt, std::nullopt, std::nullopt, {std::nullopt, false, "Synthetic fixture"}, true};
    r.table.table_id = "TABLE_R";
    r.table.region_x_min = 20; r.table.region_y_min = 80; r.table.region_x_max = 690; r.table.region_y_max = 215;
    r.table.header_y_min = 80; r.table.header_y_max = 110; r.table.row_y_tolerance = 2;
    r.table.parameter_column = {20, 215}; r.table.unit_column = {215, 285}; r.table.minimum_column = {285, 350};
    r.table.typical_column = {350, 420}; r.table.maximum_column = {420, 490}; r.table.condition_column = pdf::HorizontalSpan{490, 690};
    r.table.category = component::ParameterCategory::electrical_characteristic;
    return r;
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        require(QFile::exists(QStringLiteral(":/datasim/branding/datasim-mark.svg")), "Qt resource path resolves");
        require(!QFile::exists(QStringLiteral(":/datasim/missing-resource.svg")), "invalid Qt resource path stays absent");
        QPdfDocument document;
        require(document.load(QStringLiteral(DATASIM_SYNTHETIC_PDF_PATH)) == QPdfDocument::Error::None, "synthetic PDF opens");
        require(document.status() == QPdfDocument::Status::Ready && document.pageCount() == 2, "page count is available");
        require(document.pagePointSize(0).width() > 0, "page geometry is readable");
        QPdfView view;
        view.setDocument(&document);
        view.pageNavigator()->jump(1, {});
        require(view.pageNavigator()->currentPage() == 1, "page navigation uses zero-based index");
        QPdfDocument image_only;
        require(image_only.load(QStringLiteral(DATASIM_IMAGE_ONLY_PDF_PATH)) == QPdfDocument::Error::None, "image-only PDF opens for display");
        QTemporaryDir temporary;
        require(temporary.isValid(), "temporary directory created");
        const auto corrupt_path = temporary.filePath("corrupt.pdf");
        { std::ofstream corrupt(corrupt_path.toStdString(), std::ios::binary); corrupt << "not a PDF"; }
        QPdfDocument corrupt;
        require(corrupt.load(corrupt_path) != QPdfDocument::Error::None, "corrupt PDF reports error");
        QPdfDocument missing;
        require(missing.load(temporary.filePath("missing.pdf")) != QPdfDocument::Error::None, "missing PDF reports error");
        datasim_viewer::EvidenceWorkbench widget;
        require(widget.findChild<QLineEdit*>("evidenceField0") != nullptr, "explicit configuration form is present");
        auto request = extraction_request();
        require(!request.pdfinfo_path.empty() && !request.pdftotext_path.empty(), "Poppler tools found for synthetic extraction");
        const auto extracted = datasim::pdf::extract_electrical_table(request);
        require(extracted.candidate_bundle.has_value(), "synthetic resistor table produces candidate bundle");
        require(extracted.candidate_bundle->observations.size() == 9, "minimum typical maximum remain separate observations");
        require(extracted.candidate_bundle->observations.front().review_state == datasim::component::ReviewState::candidate, "extraction does not approve observations");
        require(!extracted.candidate_bundle->knowledge.has_value() && extracted.candidate_bundle->models.empty(), "extraction creates no knowledge or models");
        require(extracted.candidate_bundle->observations.front().original_value_text == "9.9" && extracted.candidate_bundle->observations.front().original_unit_symbol == "kohm", "source text and unit are preserved");
        const auto repeated = datasim::pdf::extract_electrical_table(request);
        require(repeated.candidate_bundle == extracted.candidate_bundle, "repeated extraction is deterministic");
        request.pdf_page_index = 2;
        require(!datasim::pdf::extract_electrical_table(request).candidate_bundle, "invalid PDF page index fails closed");
        request = extraction_request();
        request.pdfinfo_path = "missing-pdfinfo-datasim";
        require(!datasim::pdf::extract_electrical_table(request).candidate_bundle, "missing Poppler executable fails closed");
        request = extraction_request();
        QThread worker_thread;
        auto* worker = new datasim_viewer::ExtractionWorker(request);
        worker->moveToThread(&worker_thread);
        QEventLoop delivery_loop;
        std::shared_ptr<datasim::pdf::ExtractionResult> worker_result;
        bool delivered_on_gui_thread = false;
        QObject::connect(&worker_thread, &QThread::started, worker, &datasim_viewer::ExtractionWorker::run);
        QObject::connect(worker, &datasim_viewer::ExtractionWorker::completed, &worker_thread, &QThread::quit, Qt::DirectConnection);
        QObject::connect(worker, &datasim_viewer::ExtractionWorker::completed, &delivery_loop,
            [&](std::shared_ptr<datasim::pdf::ExtractionResult> result) {
                worker_result = std::move(result);
                delivered_on_gui_thread = QThread::currentThread() == app.thread();
                delivery_loop.quit();
            }, Qt::QueuedConnection);
        QObject::connect(&worker_thread, &QThread::finished, worker, &QObject::deleteLater);
        worker_thread.start();
        delivery_loop.exec();
        worker_thread.wait();
        require(worker_result && worker_result->candidate_bundle.has_value(), "worker returns synthetic candidate extraction");
        require(delivered_on_gui_thread, "worker result arrives through queued GUI-thread delivery");
        std::cout << "evidence workbench: 22 checks passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
