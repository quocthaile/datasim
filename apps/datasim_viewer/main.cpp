#include "read_only_samples.hpp"
#include "simulation_worker.hpp"
#include "waveform_widget.hpp"
#include "workbench_service.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QScrollArea>
#include <QStatusBar>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>
#include <array>
#include <charconv>
#include <filesystem>

namespace {
QString to_qstring(const std::filesystem::path& path) {
    return QString::fromStdWString(path.wstring());
}

QString number_text(double value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
    if (result.ec != std::errc{}) return QString::number(value, 'g', 17);
    return QString::fromLatin1(buffer, static_cast<qsizetype>(result.ptr - buffer));
}

QString artifact_summary(const QString& directory, const QStringList& artifacts) {
    return QString("Output: %1\nExisting artifacts: %2")
        .arg(directory, artifacts.isEmpty() ? QStringLiteral("(none)") : artifacts.join(", "));
}

class WorkbenchWindow final : public QMainWindow {
public:
    WorkbenchWindow() {
        setWindowTitle("Datasim Viewer — Manual Ideal-RC Simulation");
        resize(1320, 820);
        build_ui();
        set_state("Idle", "Manual Ideal-RC Simulation. Enter values in SI units using decimal/scientific notation.");
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        if (thread_) {
            close_when_idle_ = true;
            state_label_->setText("Running — waiting for ngspice and validation to finish before closing");
            event->ignore();
            return;
        }
        event->accept();
    }

private:
    struct Field {
        const char* name;
        const char* label;
        double datasim::RcConfig::*member;
    };

    static constexpr std::array<Field, 11> fields_{{
        {"resistance_ohm", "resistance_ohm (ohm)", &datasim::RcConfig::resistance_ohm},
        {"capacitance_F", "capacitance_F (F)", &datasim::RcConfig::capacitance_F},
        {"voltage_V", "voltage_V (V)", &datasim::RcConfig::voltage_V},
        {"initial_voltage_V", "initial_voltage_V (V)", &datasim::RcConfig::initial_voltage_V},
        {"delay_s", "delay_s (s)", &datasim::RcConfig::delay_s},
        {"rise_s", "rise_s (s)", &datasim::RcConfig::rise_s},
        {"fall_s", "fall_s (s)", &datasim::RcConfig::fall_s},
        {"width_s", "width_s (s)", &datasim::RcConfig::width_s},
        {"period_s", "period_s (s)", &datasim::RcConfig::period_s},
        {"step_s", "step_s (s)", &datasim::RcConfig::step_s},
        {"duration_s", "duration_s (s)", &datasim::RcConfig::duration_s},
    }};

    void build_ui() {
        auto* central = new QWidget(this);
        auto* root = new QHBoxLayout(central);
        auto* scroll = new QScrollArea(central);
        scroll->setWidgetResizable(true);
        scroll->setMinimumWidth(360);
        auto* controls = new QWidget(scroll);
        auto* side = new QVBoxLayout(controls);

        auto* title = new QLabel("Manual Ideal-RC Simulation", controls);
        QFont title_font = title->font();
        title_font.setBold(true);
        title_font.setPointSize(title_font.pointSize() + 2);
        title->setFont(title_font);
        side->addWidget(title);

        auto* config_box = new QGroupBox("Manual RC configuration (SI units)", controls);
        auto* form = new QFormLayout(config_box);
        datasim::RcConfig defaults;
        for (std::size_t i = 0; i < fields_.size(); ++i) {
            auto* edit = new QLineEdit(number_text(defaults.*(fields_[i].member)), config_box);
            edit->setObjectName(QString::fromLatin1(fields_[i].name));
            edit->setToolTip("Strict decimal/scientific notation; '.' decimal point; no units or separators.");
            edit->setMaxLength(64);
            field_edits_[i] = edit;
            form->addRow(QString::fromLatin1(fields_[i].label), edit);
        }
        side->addWidget(config_box);

        auto* backend_box = new QGroupBox("Execution", controls);
        auto* backend_form = new QFormLayout(backend_box);
        auto* backend_row = new QWidget(backend_box);
        auto* backend_layout = new QHBoxLayout(backend_row);
        backend_layout->setContentsMargins(0, 0, 0, 0);
        executable_edit_ = new QLineEdit(qEnvironmentVariable("DATASIM_NGSPICE", "ngspice"), backend_row);
        executable_button_ = new QPushButton("Browse…", backend_row);
        backend_layout->addWidget(executable_edit_);
        backend_layout->addWidget(executable_button_);
        backend_form->addRow("ngspice executable", backend_row);
        auto* output_row = new QWidget(backend_box);
        auto* output_layout = new QHBoxLayout(output_row);
        output_layout->setContentsMargins(0, 0, 0, 0);
        output_parent_edit_ = new QLineEdit(QDir::homePath(), output_row);
        output_button_ = new QPushButton("Browse…", output_row);
        output_layout->addWidget(output_parent_edit_);
        output_layout->addWidget(output_button_);
        backend_form->addRow("Output parent", output_row);
        side->addWidget(backend_box);

        tau_label_ = new QLabel("RC time constant: (validate configuration to calculate)", controls);
        side->addWidget(tau_label_);
        state_label_ = new QLabel(controls);
        state_label_->setWordWrap(true);
        side->addWidget(state_label_);
        info_label_ = new QLabel(controls);
        info_label_->setWordWrap(true);
        info_label_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        side->addWidget(info_label_);

        auto* buttons = new QHBoxLayout();
        run_button_ = new QPushButton("Run simulation", controls);
        open_button_ = new QPushButton("Open CSV…", controls);
        buttons->addWidget(run_button_);
        buttons->addWidget(open_button_);
        side->addLayout(buttons);
        side->addStretch(1);
        scroll->setWidget(controls);

        plot_ = new datasim_viewer::WaveformWidget(central);
        root->addWidget(scroll, 0);
        root->addWidget(plot_, 1);
        setCentralWidget(central);

        input_checkbox_ = new QCheckBox("Show input voltage", this);
        statusBar()->addPermanentWidget(input_checkbox_);
        connect(input_checkbox_, &QCheckBox::toggled, plot_, [this](bool show) { plot_->set_show_input(show); });
        connect(run_button_, &QPushButton::clicked, this, [this] { start_run(); });
        connect(open_button_, &QPushButton::clicked, this, [this] { open_csv(); });
        connect(executable_button_, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getOpenFileName(this, "Select ngspice executable", executable_edit_->text());
            if (!path.isEmpty()) executable_edit_->setText(path);
        });
        connect(output_button_, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getExistingDirectory(this, "Select existing output parent", output_parent_edit_->text());
            if (!path.isEmpty()) output_parent_edit_->setText(path);
        });
    }

    void set_state(const QString& state, const QString& details) {
        state_label_->setText(state + ": " + details);
        statusBar()->showMessage(state + ": " + details);
    }

    void set_busy(bool busy) {
        run_button_->setEnabled(!busy);
        open_button_->setEnabled(!busy);
        input_checkbox_->setEnabled(!busy);
        for (auto* edit : field_edits_) edit->setEnabled(!busy);
        executable_edit_->setEnabled(!busy);
        output_parent_edit_->setEnabled(!busy);
        executable_button_->setEnabled(!busy);
        output_button_->setEnabled(!busy);
    }

    std::array<std::string, 11> field_values() const {
        std::array<std::string, 11> values;
        for (std::size_t i = 0; i < field_edits_.size(); ++i) {
            const QByteArray bytes = field_edits_[i]->text().toLatin1();
            if (QString::fromLatin1(bytes) != field_edits_[i]->text())
                throw std::invalid_argument(std::string(fields_[i].name) + ": ASCII numeric notation is required");
            values[i] = bytes.toStdString();
        }
        return values;
    }

    void start_run() {
        set_state("Validating", "checking all entered values with datasim::check_config()");
        plot_->clear();
        info_label_->clear();
        try {
            const auto config = datasim_viewer::parse_rc_config(field_values());
            const auto parent = std::filesystem::path(output_parent_edit_->text().toStdWString());
            // Path selection is checked after configuration validation and does not create a directory.
            const auto output = datasim_viewer::fresh_run_directory(parent);
            const auto executable = executable_edit_->text().toStdString();
            if (executable.empty()) throw std::invalid_argument("ngspice executable path is empty");
            last_config_ = config;
            tau_label_->setText("RC time constant tau = " + number_text(datasim::time_constant(config)) + " s");
            info_label_->setText("Run output directory: " + to_qstring(output) + "\nNo output directory is created until execution begins.");
            set_state("Running", "ngspice execution and datasim numerical validation are active in a worker thread");
            set_busy(true);
            input_checkbox_->setEnabled(false);
            launch_worker(config, executable, output);
        } catch (const std::exception& error) {
            tau_label_->setText("RC time constant: (not available until configuration passes validation)");
            set_state("Failed", QString::fromUtf8(error.what()));
        }
    }

    void launch_worker(const datasim::RcConfig& config, const std::string& executable,
                       const std::filesystem::path& output) {
        qRegisterMetaType<datasim::RunResult>("datasim::RunResult");
        auto* thread = new QThread(this);
        auto* worker = new datasim_viewer::SimulationWorker(config, executable, output);
        worker->moveToThread(thread);
        thread_ = thread;
        connect(thread, &QThread::started, worker, &datasim_viewer::SimulationWorker::execute);
        connect(worker, &datasim_viewer::SimulationWorker::succeeded, this,
                [this](datasim::RunResult result, const QString& directory, const QStringList& artifacts) {
            // This slot runs on the GUI thread. datasim is the authority for numerical metrics.
            if (!result.validation.passed) {
                on_failed("datasim returned validation.passed=false", directory, artifacts);
                return;
            }
            const auto sample_count = result.samples.size();
            plot_->set_data(std::move(result.samples), std::filesystem::path(directory.toStdWString()));
            info_label_->setText(QString("Manual Ideal-RC Simulation — numerical validation PASSED\n"
                                         "R = %1 ohm; C = %2 F; tau = %3 s\n"
                                         "Samples: %4\nBackend time: %5 s\n"
                                         "Maximum absolute error: %6 V\nRMSE: %7 V\nTolerance: %8 V\n%9")
                .arg(number_text(last_config_.resistance_ohm), number_text(last_config_.capacitance_F),
                     number_text(datasim::time_constant(last_config_)))
                .arg(static_cast<qulonglong>(sample_count))
                .arg(number_text(result.backend_seconds), number_text(result.validation.max_absolute_error_V),
                     number_text(result.validation.rmse_V), number_text(result.validation.tolerance_V),
                     artifact_summary(directory, artifacts)));
            set_state("Succeeded", "ngspice completed and datasim numerical validation passed");
        }, Qt::QueuedConnection);
        connect(worker, &datasim_viewer::SimulationWorker::failed, this,
                [this](const QString& error, const QString& directory, const QStringList& artifacts) {
            on_failed(error, directory, artifacts);
        }, Qt::QueuedConnection);
        connect(worker, &datasim_viewer::SimulationWorker::finished, thread, &QThread::quit);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished, this, [this, thread] {
            if (thread_ == thread) thread_ = nullptr;
            set_busy(false);
            input_checkbox_->setEnabled(true);
            thread->deleteLater();
            if (close_when_idle_) close();
        });
        thread->start();
    }

    void on_failed(const QString& error, const QString& directory, const QStringList& artifacts) {
        plot_->set_error("Run failed; prior data has been cleared.\n" + error);
        info_label_->setText("Manual Ideal-RC Simulation — FAILED\n" + artifact_summary(directory, artifacts));
        set_state("Failed", error);
    }

    void open_csv() {
        const auto file = QFileDialog::getOpenFileName(this, "Open existing Datasim results.csv", {},
                                                       "CSV files (*.csv);;All files (*)");
        if (file.isEmpty()) return;
        const std::filesystem::path path(file.toStdWString());
        try {
            auto loaded = datasim_viewer::load_samples_csv(path);
            const auto count = loaded.samples.size();
            plot_->set_data(std::move(loaded.samples), loaded.source_path);
            info_label_->setText(QString("Loaded CSV in read-only mode; this is not a newly executed simulation.\n"
                                         "Source: %1\nSamples: %2")
                                     .arg(to_qstring(loaded.source_path)).arg(static_cast<qulonglong>(count)));
            set_state("Idle", "CSV loaded read-only; it is not labeled as a simulation result from this session");
            tau_label_->setText("RC time constant: (run manual configuration to calculate)");
        } catch (const std::exception& error) {
            plot_->set_error(QString::fromUtf8(error.what()));
            info_label_->setText("CSV load failed; source was opened read-only.");
            set_state("Failed", QString::fromUtf8(error.what()));
        }
    }

    std::array<QLineEdit*, 11> field_edits_{};
    QLineEdit* executable_edit_ = nullptr;
    QLineEdit* output_parent_edit_ = nullptr;
    QPushButton* run_button_ = nullptr;
    QPushButton* open_button_ = nullptr;
    QPushButton* executable_button_ = nullptr;
    QPushButton* output_button_ = nullptr;
    QCheckBox* input_checkbox_ = nullptr;
    QLabel* tau_label_ = nullptr;
    QLabel* state_label_ = nullptr;
    QLabel* info_label_ = nullptr;
    datasim_viewer::WaveformWidget* plot_ = nullptr;
    QThread* thread_ = nullptr;
    bool close_when_idle_ = false;
    datasim::RcConfig last_config_{};
};
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Datasim Viewer");
    WorkbenchWindow window;
    window.show();
    return app.exec();
}
