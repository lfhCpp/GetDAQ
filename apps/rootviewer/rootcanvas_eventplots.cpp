#include "rootcanvas.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>

void EventCanvas::DrawAxes(QPainter &painter, const QRectF &plot, const QString &yTitle) {
        painter.setPen(QPen(QColor(215, 220, 226), 1));
        for (int i = 0; i <= 4; ++i) {
            const qreal x = plot.left() + plot.width() * i / 4.0;
            painter.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
        for (int i = 0; i <= 4; ++i) {
            const qreal y = plot.top() + plot.height() * i / 4.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        painter.setPen(QColor(45, 48, 52));
        painter.drawText(QRectF(plot.left(), plot.bottom() + 3, plot.width(), 18), Qt::AlignCenter,
                         tr("Time bucket"));
        painter.save();
        painter.translate(plot.left() - 27, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -9, plot.height(), 18), Qt::AlignCenter, yTitle);
        painter.restore();
    
}

void EventCanvas::DrawWaveforms(QPainter &painter, const QRectF &panel, int aget) {
        painter.setPen(QColor(30, 35, 40));
        painter.drawText(QRectF(panel.left(), panel.top(), panel.width(), 20), Qt::AlignCenter,
                         tr("AGET %1 — channel waveforms").arg(aget));
        const QRectF plot = panel.adjusted(38, 23, -8, -24);
        DrawAxes(painter, plot, tr("ADC"));
        double minimum = 0.0;
        double maximum = 1.0;
        bool hasSamples = false;
        for (int channel = 0; channel < 68; ++channel) {
            for (const GetDAQ::RootSample &sample : fEvent.channels[aget][channel]) {
                if (!hasSamples) {
                    minimum = maximum = sample.value;
                    hasSamples = true;
                } else {
                    minimum = qMin(minimum, double(sample.value));
                    maximum = qMax(maximum, double(sample.value));
                }
            }
        }
        if (!hasSamples) return;
        if (maximum <= minimum) {
            minimum -= 0.5;
            maximum += 0.5;
        } else {
            const double margin = (maximum - minimum) * 0.05;
            minimum -= margin;
            maximum += margin;
        }
        painter.save();
        painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
        for (int channel = 0; channel < 68; ++channel) {
            const std::vector<GetDAQ::RootSample> &samples = fEvent.channels[aget][channel];
            if (samples.empty()) continue;
            QPainterPath path;
            for (std::size_t i = 0; i < samples.size(); ++i) {
                const qreal x = plot.left() + plot.width() * samples[i].bucket / 511.0;
                const qreal y = plot.bottom() - plot.height() * (samples[i].value - minimum) / (maximum - minimum);
                if (!i) path.moveTo(x, y); else path.lineTo(x, y);
            }
            painter.setPen(QPen(QColor::fromHsv((channel * 360) / 68, 190, 170), 1.0));
            painter.drawPath(path);
        }
        painter.restore();
    
}

void EventCanvas::DrawHeatmap(QPainter &painter, const QRectF &panel, int aget) {
        painter.setPen(QColor(30, 35, 40));
        painter.drawText(QRectF(panel.left(), panel.top(), panel.width(), 20), Qt::AlignCenter,
                         tr("AGET %1 — bucket × channel intensity").arg(aget));
        const QRectF plot = panel.adjusted(38, 23, -8, -24);
        QImage image(512, 68, QImage::Format_RGB32);
        image.fill(QColor(238, 240, 243));
        unsigned maximum = 1;
        for (int channel = 0; channel < 68; ++channel)
            for (const GetDAQ::RootSample &sample : fEvent.channels[aget][channel])
                maximum = qMax(maximum, unsigned(sample.value));
        for (int channel = 0; channel < 68; ++channel) {
            for (const GetDAQ::RootSample &sample : fEvent.channels[aget][channel]) {
                const int hue = 240 - int(240.0 * sample.value / maximum);
                image.setPixelColor(sample.bucket, 67 - channel, QColor::fromHsv(hue, 235, 245));
            }
        }
        painter.save();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        painter.drawImage(plot, image);
        painter.restore();
        DrawAxes(painter, plot, tr("Channel"));
        painter.setPen(QColor(45, 48, 52));
        painter.drawText(QRectF(panel.right() - 92, panel.top() + 2, 88, 18), Qt::AlignRight,
                         tr("max ADC %1").arg(maximum));
    
}
