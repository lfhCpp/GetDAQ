// 3D AGET surface projection and painter path for the ROOT event canvas.
#include "rootcanvas.h"

#include <QPainter>

#include <algorithm>
#include <cmath>

QPointF EventCanvas::ProjectSurfacePoint(double bucket, double channel, double value, double maximum,
                                          const QRectF &plot, double *depth) const {
        const double x = bucket / 511.0 - 0.5;
        const double y = channel / 67.0 - 0.5;
        const double z = maximum > 0.0 ? value / maximum : 0.0;
        const double cosine = std::cos(fYaw);
        const double sine = std::sin(fYaw);
        const double horizontal = x * cosine - y * sine;
        const double rotatedDepth = x * sine + y * cosine;
        const double vertical = rotatedDepth * std::sin(fElevation) + (z - 0.5) * std::cos(fElevation);
        if (depth) *depth = rotatedDepth * std::cos(fElevation) - (z - 0.5) * std::sin(fElevation);
        const double scale = qMin(plot.width() / 1.55, plot.height() / 1.45);
        return QPointF(plot.center().x() + horizontal * scale, plot.center().y() - vertical * scale);
    
}

void EventCanvas::DrawSurface(QPainter &painter, const QRectF &panel, int aget) {
        painter.setPen(QColor(30, 35, 40));
        painter.drawText(QRectF(panel.left(), panel.top(), panel.width(), 20), Qt::AlignCenter,
                         tr("AGET %1 — bucket × channel × ADC").arg(aget));
        const QRectF plot = panel.adjusted(32, 24, -8, -22);
        std::vector<std::vector<double> > values(68, std::vector<double>(512, 0.0));
        unsigned maximum = 0;
        for (int channel = 0; channel < 68; ++channel) {
            for (const GetDAQ::RootSample &sample : fEvent.channels[aget][channel]) {
                values[channel][sample.bucket] = sample.value;
                maximum = qMax(maximum, unsigned(sample.value));
            }
        }
        std::vector<SurfaceCell> cells;
        cells.reserve(static_cast<std::size_t>(67 * 511));
        for (int channel = 0; channel < 67; ++channel) {
            for (int bucket = 0; bucket < 511; ++bucket) {
                const int nextBucket = bucket + 1;
                double depths[4] = {};
                SurfaceCell cell;
                cell.points[0] = ProjectSurfacePoint(bucket, channel, values[channel][bucket], maximum, plot, &depths[0]);
                cell.points[1] = ProjectSurfacePoint(nextBucket, channel, values[channel][nextBucket], maximum, plot, &depths[1]);
                cell.points[2] = ProjectSurfacePoint(nextBucket, channel + 1, values[channel + 1][nextBucket], maximum, plot, &depths[2]);
                cell.points[3] = ProjectSurfacePoint(bucket, channel + 1, values[channel + 1][bucket], maximum, plot, &depths[3]);
                const double average = (values[channel][bucket] + values[channel][nextBucket] +
                                        values[channel + 1][nextBucket] + values[channel + 1][bucket]) / 4.0;
                cell.depth = (depths[0] + depths[1] + depths[2] + depths[3]) / 4.0;
                cell.color = QColor::fromHsv(maximum ? 240 - int(240.0 * average / maximum) : 240, 205, 220);
                cells.push_back(cell);
            }
        }
        std::sort(cells.begin(), cells.end(), [](const SurfaceCell &left, const SurfaceCell &right) {
            return left.depth > right.depth;
        });
        painter.save();
        painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
        painter.setPen(Qt::NoPen);
        for (const SurfaceCell &cell : cells) {
            painter.setBrush(cell.color);
            painter.drawPolygon(cell.points, 4);
        }
        painter.restore();
        painter.setPen(QColor(45, 48, 52));
        painter.drawText(QRectF(panel.left(), panel.bottom() - 20, panel.width(), 18), Qt::AlignCenter,
                         tr("drag to rotate  |  max ADC %1").arg(maximum));
    
}
