#include "read_only_samples.hpp"
#include "waveform_widget.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>
#include <QString>

namespace {
QString to_qstring(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Datasim Viewer");
    QMainWindow window;
    window.setWindowTitle("Datasim Viewer — Read-only Results");
    window.resize(1100, 680);

    auto* plot = new datasim_viewer::WaveformWidget(&window);
    window.setCentralWidget(plot);
    auto* toolbar = window.addToolBar("Results");
    auto* open_action = toolbar->addAction("Open results.csv…");
    auto* input_checkbox = new QCheckBox("Show input voltage", toolbar);
    input_checkbox->setToolTip("Overlay Vin from the loaded Samples; this does not modify results.");
    toolbar->addWidget(input_checkbox);
    QObject::connect(input_checkbox, &QCheckBox::toggled, plot, [plot](bool checked) {
        plot->set_show_input(checked);
    });
    QObject::connect(open_action, &QAction::triggered, &window, [&window, plot]() {
        const auto file = QFileDialog::getOpenFileName(&window, "Open Datasim results", {},
                                                        "CSV files (*.csv);;All files (*)");
        if (file.isEmpty()) return;
        const std::filesystem::path path(file.toStdWString());
        try {
            auto loaded = datasim_viewer::load_samples_csv(path);
            const auto sample_count = loaded.samples.size();
            plot->set_data(std::move(loaded.samples), loaded.source_path);
            window.statusBar()->showMessage(
                QString("Loaded %1 samples from %2. Input opened read-only.")
                    .arg(sample_count)
                    .arg(to_qstring(loaded.source_path)));
        } catch (const std::exception& error) {
            const auto message = QString::fromUtf8(error.what());
            plot->set_error(message);
            window.statusBar()->showMessage(message);
        }
    });
    window.statusBar()->showMessage("No results loaded. Open a local results.csv file.");
    window.show();
    return app.exec();
}
