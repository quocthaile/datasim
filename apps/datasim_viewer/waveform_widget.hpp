#pragma once

#include "datasim/simulation/io/samples.hpp"
#include <QWidget>
#include <filesystem>
#include <optional>

class QCheckBox;

namespace datasim_viewer {

class WaveformWidget final : public QWidget {
public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    void set_data(datasim::Samples samples, std::filesystem::path source);
    void set_error(QString message);
    void set_show_input(bool show);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    datasim::Samples samples_;
    std::filesystem::path source_;
    QString error_;
    bool show_input_ = false;
};

} // namespace datasim_viewer
