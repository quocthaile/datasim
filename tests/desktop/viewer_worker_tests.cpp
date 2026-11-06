#include "simulation_worker.hpp"
#include "workbench_service.hpp"

#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <filesystem>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    try {
        QCoreApplication app(argc, argv);
        if (argc != 3) throw std::runtime_error("expected ngspice executable and isolated output directory");
        if (std::string(argv[1]).empty() || !std::filesystem::exists(argv[1])) return 77;
        const auto parent = std::filesystem::path(argv[2]);
        std::filesystem::create_directories(parent);
        const auto output = datasim_viewer::fresh_run_directory(parent);
        QThread thread;
        QEventLoop completion;
        bool succeeded = false;
        bool callback_on_main_thread = false;
        QString failure;
        qRegisterMetaType<datasim::RunResult>("datasim::RunResult");
        auto* worker = new datasim_viewer::SimulationWorker(datasim::RcConfig{}, argv[1], output);
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::started, worker, &datasim_viewer::SimulationWorker::execute);
        QObject::connect(worker, &datasim_viewer::SimulationWorker::succeeded, &app,
            [&](datasim::RunResult result, const QString&, const QStringList&) {
                callback_on_main_thread = QThread::currentThread() == app.thread();
                succeeded = result.validation.passed && !result.samples.empty();
            }, Qt::QueuedConnection);
        QObject::connect(worker, &datasim_viewer::SimulationWorker::failed, &app,
            [&](const QString& error, const QString&, const QStringList&) { failure = error; }, Qt::QueuedConnection);
        QObject::connect(worker, &datasim_viewer::SimulationWorker::finished, &thread, &QThread::quit);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        QObject::connect(&thread, &QThread::finished, &completion, &QEventLoop::quit);
        thread.start();
        completion.exec();
        thread.wait();
        if (!failure.isEmpty()) throw std::runtime_error(failure.toStdString());
        if (!succeeded || !callback_on_main_thread)
            throw std::runtime_error("worker did not deliver a validated result to the main event thread");
        if (!std::filesystem::is_regular_file(output / "validation_report.json"))
            throw std::runtime_error("worker did not preserve the core run validation artifact");
        std::cout << "PASS: Qt worker-thread execution and queued validated result delivery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "viewer worker test failure: " << error.what() << '\n';
        return 1;
    }
}
