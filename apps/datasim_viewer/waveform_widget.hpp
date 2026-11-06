#pragma once

#include "datasim/simulation/io/samples.hpp"
#include <QWidget>
#include <filesystem>
#include <optional>
#include <vector>

class QCheckBox;

namespace datasim_viewer {

// Returns source indices for a display-only envelope bounded by the plot width.
[[nodiscard]] std::vector<std::size_t> display_sample_indices(
    const datasim::Samples& samples, std::size_t pixel_width, bool include_input);

class WaveformWidget final : public QWidget {
public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    void set_data(datasim::Samples samples, std::filesystem::path source);
    void set_error(QString message);
    void clear();
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
