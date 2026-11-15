#include "rootcanvas.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

EventCanvas::EventCanvas(QWidget *parent) : QWidget(parent), fHasEvent(false), fHeatmap(false), fSurface(false), fHasHistogram(false),
          fYaw(-0.72), fElevation(0.62), fDragging(false) {
        setMinimumSize(760, 520);
        setAutoFillBackground(true);
    
}

void EventCanvas::SetEvent(const GetDAQ::RootEvent &event, bool heatmap) {
        fEvent = event;
        fHasEvent = true;
        fHeatmap = heatmap;
        fSurface = false;
        fHasHistogram = false;
        update();
    
}

void EventCanvas::SetSurface(const GetDAQ::RootEvent &event) {
        fEvent = event;
        fHasEvent = true;
        fHeatmap = false;
        fSurface = true;
        fHasHistogram = false;
        update();
    
}

void EventCanvas::SetHistogram(const RootHistogram &histogram) {
        fHistogram = histogram;
        fSurface = false;
        fHasHistogram = true;
        update();
    
}

void EventCanvas::paintEvent(QPaintEvent *) {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(250, 250, 250));
        if (fHasHistogram) {
            DrawHistogram(painter);
            return;
        }
        if (!fHasEvent) {
            painter.drawText(rect(), Qt::AlignCenter, tr("Open a legacy ROOT event file"));
            return;
        }
        const qreal gap = 18.0;
        const qreal top = 34.0;
        const qreal bottom = 20.0;
        const qreal panelWidth = (this->width() - 3.0 * gap) / 2.0;
        const qreal panelHeight = (this->height() - top - bottom - gap) / 2.0;
        painter.setPen(QColor(35, 40, 48));
        if (fEvent.format == GetDAQ::RootEvent::Format::T2KMatrix) {
            painter.drawText(QRectF(0, 4, this->width(), 22), Qt::AlignCenter,
                             tr("T2K matrix entry  |  %1 channels × 512 buckets")
                                 .arg(fEvent.channelCount));
        } else {
            painter.drawText(QRectF(0, 4, this->width(), 22), Qt::AlignCenter,
                             tr("Event %1  |  CoBo %2 / AsAd %3  |  %4 channels, %5 samples")
                                 .arg(fEvent.event).arg(fEvent.cobo).arg(fEvent.asad)
                                 .arg(fEvent.channelCount).arg(fEvent.sampleCount));
        }
        if (fSurface) {
            painter.setPen(QColor(35, 40, 48));
            painter.drawText(QRectF(0, 4, this->width(), 22), Qt::AlignCenter,
                             tr("3D surface — drag to rotate; each panel is one AGET"));
            for (int aget = 0; aget < 4; ++aget) {
                const int column = aget % 2;
                const int row = aget / 2;
                const QRectF panel(gap + column * (panelWidth + gap), top + row * (panelHeight + gap),
                                   panelWidth, panelHeight);
                DrawSurface(painter, panel, aget);
            }
            return;
        }
        for (int aget = 0; aget < 4; ++aget) {
            const int column = aget % 2;
            const int row = aget / 2;
            QRectF panel(gap + column * (panelWidth + gap), top + row * (panelHeight + gap), panelWidth, panelHeight);
            if (fHeatmap) DrawHeatmap(painter, panel, aget);
            else DrawWaveforms(painter, panel, aget);
        }
    
}

void EventCanvas::mousePressEvent(QMouseEvent *event) {
        if (fSurface && event->button() == Qt::LeftButton) {
            fDragging = true;
            fLastMousePosition = event->pos();
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    
}

void EventCanvas::mouseMoveEvent(QMouseEvent *event) {
        if (fSurface && fDragging) {
            const QPoint delta = event->pos() - fLastMousePosition;
            fLastMousePosition = event->pos();
            fYaw += delta.x() * 0.012;
            fElevation = qBound(-1.2, fElevation + delta.y() * 0.008, 1.2);
            update();
            event->accept();
            return;
        }
        QWidget::mouseMoveEvent(event);
    
}

void EventCanvas::mouseReleaseEvent(QMouseEvent *event) {
        if (fSurface && event->button() == Qt::LeftButton && fDragging) {
            fDragging = false;
            event->accept();
            return;
        }
        QWidget::mouseReleaseEvent(event);
    
}

void EventCanvas::DrawHistogram(QPainter &painter) {
        const QRectF plot = QRectF(rect()).adjusted(82, 52, -28, -66);
        painter.setPen(QColor(35, 40, 48));
        painter.drawText(QRectF(12, 8, width() - 24, 30), Qt::AlignCenter, fHistogram.title);
        painter.setPen(QPen(QColor(215, 220, 226), 1));
        for (int i = 0; i <= 5; ++i) {
            const qreal y = plot.bottom() - plot.height() * i / 5.0;
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        const unsigned long long maxCount = fHistogram.bins.empty() ? 0 :
            *std::max_element(fHistogram.bins.begin(), fHistogram.bins.end());
        if (maxCount) {
            const qreal binWidth = plot.width() / fHistogram.bins.size();
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(55, 120, 190));
            for (std::size_t i = 0; i < fHistogram.bins.size(); ++i) {
                if (!fHistogram.bins[i]) continue;
                const qreal barHeight = plot.height() * double(fHistogram.bins[i]) / maxCount;
                painter.drawRect(QRectF(plot.left() + binWidth * i, plot.bottom() - barHeight,
                                        qMax<qreal>(1.0, binWidth - 0.5), barHeight));
            }
        }
        painter.setPen(QColor(45, 48, 52));
        painter.drawLine(plot.bottomLeft(), plot.bottomRight());
        painter.drawLine(plot.bottomLeft(), plot.topLeft());
        painter.drawText(QRectF(plot.left(), plot.bottom() + 8, plot.width(), 24), Qt::AlignCenter,
                         fHistogram.xTitle);
        painter.save();
        painter.translate(24, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -10, plot.height(), 20), Qt::AlignCenter,
                         tr("Entries per bin"));
        painter.restore();
        painter.drawText(QRectF(plot.left(), plot.bottom() + 32, plot.width(), 20), Qt::AlignCenter,
                         tr("Range: %1 to %2").arg(fHistogram.minimum, 0, 'g', 6)
                             .arg(fHistogram.maximum, 0, 'g', 6));
    
}
