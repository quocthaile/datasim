#include "waveform_widget.hpp"

#include <QCheckBox>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <algorithm>
#include <cmath>

namespace datasim_viewer {
namespace {
constexpr int left_margin = 82;
constexpr int right_margin = 28;
constexpr int top_margin = 42;
constexpr int bottom_margin = 68;
}

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(640, 360);
    setAutoFillBackground(true);
}

void WaveformWidget::set_data(datasim::Samples samples, std::filesystem::path source) {
    samples_ = std::move(samples);
    source_ = std::move(source);
    error_.clear();
    update();
}

void WaveformWidget::set_error(QString message) {
    samples_.clear();
    source_.clear();
    error_ = std::move(message);
    update();
}

void WaveformWidget::set_show_input(bool show) {
    show_input_ = show;
    update();
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette().base());
    if (!error_.isEmpty()) {
        painter.setPen(QColor(255, 105, 105));
        painter.drawText(rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Unable to display results\n\n%1").arg(error_));
        return;
    }
    if (samples_.empty()) {
        painter.setPen(palette().text().color());
        painter.drawText(rect(), Qt::AlignCenter, tr("Open a Datasim results.csv file to view samples."));
        return;
    }

    const QRectF plot(left_margin, top_margin, width() - left_margin - right_margin,
                      height() - top_margin - bottom_margin);
    const auto [min_time, max_time] = std::minmax_element(samples_.begin(), samples_.end(),
        [](const auto& a, const auto& b) { return a.time_s < b.time_s; });
    double min_voltage = min_time->vout_V;
    double max_voltage = min_time->vout_V;
    for (const auto& sample : samples_) {
        min_voltage = std::min(min_voltage, sample.vout_V);
        max_voltage = std::max(max_voltage, sample.vout_V);
        if (show_input_) {
            min_voltage = std::min(min_voltage, sample.vin_V);
            max_voltage = std::max(max_voltage, sample.vin_V);
        }
    }
    if (min_voltage == max_voltage) { min_voltage -= 0.5; max_voltage += 0.5; }
    const double voltage_pad = (max_voltage - min_voltage) * 0.08;
    min_voltage -= voltage_pad;
    max_voltage += voltage_pad;
    const double time_range = max_time->time_s - min_time->time_s;
    auto point = [&](double time, double voltage) {
        const double x_fraction = time_range > 0 ? (time - min_time->time_s) / time_range : 0.5;
        const double y_fraction = (voltage - min_voltage) / (max_voltage - min_voltage);
        return QPointF(plot.left() + x_fraction * plot.width(), plot.bottom() - y_fraction * plot.height());
    };

    painter.setPen(QPen(palette().mid().color(), 1));
    for (int i = 0; i <= 4; ++i) {
        const double f = static_cast<double>(i) / 4.0;
        const double y = plot.bottom() - f * plot.height();
        const double x = plot.left() + f * plot.width();
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        painter.setPen(palette().text().color());
        painter.drawText(QRectF(4, y - 10, left_margin - 12, 20), Qt::AlignRight | Qt::AlignVCenter,
                         QString::number(min_voltage + f * (max_voltage - min_voltage), 'g', 4));
        painter.drawText(QRectF(x - 42, plot.bottom() + 6, 84, 20), Qt::AlignHCenter | Qt::AlignTop,
                         QString::number(min_time->time_s + f * time_range, 'g', 4));
        painter.setPen(QPen(palette().mid().color(), 1));
    }
    painter.setPen(QPen(palette().text().color(), 1.5));
    painter.drawLine(plot.bottomLeft(), plot.bottomRight());
    painter.drawLine(plot.topLeft(), plot.bottomLeft());
    painter.drawText(QRectF(plot.left(), height() - 31, plot.width(), 22), Qt::AlignCenter, tr("Time (s)"));
    painter.save();
    painter.translate(20, plot.center().y());
    painter.rotate(-90);
    painter.drawText(QRectF(-plot.height() / 2, -12, plot.height(), 24), Qt::AlignCenter, tr("Voltage (V)"));
    painter.restore();

    QPainterPath output_path;
    for (std::size_t i = 0; i < samples_.size(); ++i) {
        const auto p = point(samples_[i].time_s, samples_[i].vout_V);
        if (i == 0) output_path.moveTo(p); else output_path.lineTo(p);
    }
    painter.setPen(QPen(QColor(30, 100, 210), 2));
    painter.drawPath(output_path);
    if (show_input_) {
        QPainterPath input_path;
        for (std::size_t i = 0; i < samples_.size(); ++i) {
            const auto p = point(samples_[i].time_s, samples_[i].vin_V);
            if (i == 0) input_path.moveTo(p); else input_path.lineTo(p);
        }
        painter.setPen(QPen(QColor(210, 90, 35), 2));
        painter.drawPath(input_path);
    }

    const int legend_x = static_cast<int>(plot.left() + 8);
    const int legend_y = static_cast<int>(plot.top() + 8);
    painter.setPen(QPen(QColor(30, 100, 210), 2));
    painter.drawLine(legend_x, legend_y, legend_x + 24, legend_y);
    painter.setPen(palette().text().color());
    painter.drawText(legend_x + 30, legend_y + 5, tr("Output voltage Vout (V)"));
    if (show_input_) {
        painter.setPen(QPen(QColor(210, 90, 35), 2));
        painter.drawLine(legend_x + 210, legend_y, legend_x + 234, legend_y);
        painter.setPen(palette().text().color());
        painter.drawText(legend_x + 240, legend_y + 5, tr("Input voltage Vin (V)"));
    }
    const QString file = QString::fromStdWString(source_.wstring());
    painter.setPen(palette().text().color());
    painter.drawText(QRectF(plot.left(), 8, plot.width(), 24), Qt::AlignCenter,
                     tr("%1 | %2 samples | %3 to %4 s")
                         .arg(file).arg(samples_.size())
                         .arg(min_time->time_s, 0, 'g', 5).arg(max_time->time_s, 0, 'g', 5));
}

} // namespace datasim_viewer
