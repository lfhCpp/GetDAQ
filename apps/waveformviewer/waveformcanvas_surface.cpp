#include "waveformcanvas.h"

#include <QPainter>

#include <algorithm>
#include <cmath>
#include <vector>

namespace GetDAQ {
namespace {
struct SurfaceCell {
    QPointF points[4];
    double depth = 0.0;
    QColor color;
};
}

QPointF WaveformCanvas::ProjectSurfacePoint(double bucket, double channel, double normalizedValue,
                                            const QRectF &plot, double *depth) const
{
    const double x = bucket / 511.0 - 0.5;
    const double y = channel / 67.0 - 0.5;
    const double cosine = std::cos(fYaw);
    const double sine = std::sin(fYaw);
    const double horizontal = x * cosine - y * sine;
    const double rotatedDepth = x * sine + y * cosine;
    const double vertical = rotatedDepth * std::sin(fElevation) +
        (normalizedValue - 0.5) * std::cos(fElevation);
    if (depth) *depth = rotatedDepth * std::cos(fElevation) -
        (normalizedValue - 0.5) * std::sin(fElevation);
    const double scale = qMin(plot.width() / 1.55, plot.height() / 1.45);
    return QPointF(plot.center().x() + horizontal * scale, plot.center().y() - vertical * scale);
}

void WaveformCanvas::DrawSurface(QPainter &painter)
{
    if (!fHasData) {
        painter.drawText(rect(), Qt::AlignCenter, tr("Open a .graw or .gmat waveform file"));
        return;
    }
    const QRectF plot = QRectF(rect()).adjusted(64, 36, -46, -62);
    if (plot.width() < 20 || plot.height() < 20) return;
    std::vector<double> values(68 * 512, 0.0);
    double minimum = 0.0;
    double maximum = 0.0;
    for (const WaveformPlotTrace &trace : fTraces) {
        if (trace.channel < 0 || trace.channel >= 68) continue;
        for (const QPointF &sample : trace.samples) {
            const int bucket = qBound(0, qRound(sample.x()), 511);
            values[std::size_t(trace.channel) * 512 + std::size_t(bucket)] = sample.y();
            minimum = qMin(minimum, sample.y());
            maximum = qMax(maximum, sample.y());
        }
    }
    const double span = maximum - minimum;
    std::vector<SurfaceCell> cells;
    cells.reserve(67u * 511u);
    for (int channel = 0; channel < 67; ++channel) {
        for (int bucket = 0; bucket < 511; ++bucket) {
            const double v0 = values[std::size_t(channel) * 512 + std::size_t(bucket)];
            const double v1 = values[std::size_t(channel) * 512 + std::size_t(bucket + 1)];
            const double v2 = values[std::size_t(channel + 1) * 512 + std::size_t(bucket + 1)];
            const double v3 = values[std::size_t(channel + 1) * 512 + std::size_t(bucket)];
            const double n0 = span > 0.0 ? (v0 - minimum) / span : 0.5;
            const double n1 = span > 0.0 ? (v1 - minimum) / span : 0.5;
            const double n2 = span > 0.0 ? (v2 - minimum) / span : 0.5;
            const double n3 = span > 0.0 ? (v3 - minimum) / span : 0.5;
            double depths[4] = {};
            SurfaceCell cell;
            cell.points[0] = ProjectSurfacePoint(bucket, channel, n0, plot, &depths[0]);
            cell.points[1] = ProjectSurfacePoint(bucket + 1, channel, n1, plot, &depths[1]);
            cell.points[2] = ProjectSurfacePoint(bucket + 1, channel + 1, n2, plot, &depths[2]);
            cell.points[3] = ProjectSurfacePoint(bucket, channel + 1, n3, plot, &depths[3]);
            cell.depth = (depths[0] + depths[1] + depths[2] + depths[3]) / 4.0;
            const double average = (n0 + n1 + n2 + n3) / 4.0;
            cell.color = QColor::fromHsv(240 - int(240.0 * average), 205, 220);
            cells.push_back(cell);
        }
    }
    std::sort(cells.begin(), cells.end(), [](const SurfaceCell &left, const SurfaceCell &right) {
        return left.depth > right.depth;
    });
    painter.setPen(QColor(35, 40, 48));
    painter.drawText(QRectF(4, 4, width() - 8, 22), Qt::AlignCenter,
                     tr("AGET %1 — bucket × channel × ADC surface").arg(fAget));
    painter.save();
    painter.setClipRect(plot.adjusted(-1, -1, 1, 1));
    painter.setPen(Qt::NoPen);
    for (const SurfaceCell &cell : cells) {
        painter.setBrush(cell.color);
        painter.drawPolygon(cell.points, 4);
    }
    painter.restore();
    painter.setPen(QPen(QColor(55, 60, 66), 1));
    painter.drawText(QRectF(0, height() - 30, width(), 22), Qt::AlignCenter,
                     tr("Drag to rotate  |  signal range %1 to %2").arg(minimum).arg(maximum));
}

} // namespace GetDAQ
