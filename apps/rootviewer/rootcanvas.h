#ifndef GETDAQ_ROOTCANVAS_H
#define GETDAQ_ROOTCANVAS_H

#include "roottreereader.h"

#include <QColor>
#include <QPoint>
#include <QPointF>
#include <QWidget>

#include <vector>

struct RootHistogram {
    std::vector<unsigned long long> bins;
    double minimum = 0.0;
    double maximum = 1.0;
    QString title;
    QString xTitle;
};

class QPainter;
class QRectF;
class QPaintEvent;
class QMouseEvent;

class EventCanvas : public QWidget {
public:
    explicit EventCanvas(QWidget *parent = nullptr);
    void SetEvent(const GetDAQ::RootEvent &event, bool heatmap);
    void SetSurface(const GetDAQ::RootEvent &event);
    void SetHistogram(const RootHistogram &histogram);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void DrawHistogram(QPainter &painter);
    void DrawAxes(QPainter &painter, const QRectF &plot, const QString &yTitle);
    void DrawWaveforms(QPainter &painter, const QRectF &panel, int aget);
    void DrawHeatmap(QPainter &painter, const QRectF &panel, int aget);
    struct SurfaceCell {
        QPointF points[4];
        double depth;
        QColor color;
    };
    QPointF ProjectSurfacePoint(double bucket, double channel, double value,
                                double maximum, const QRectF &plot, double *depth) const;
    void DrawSurface(QPainter &painter, const QRectF &panel, int aget);

    GetDAQ::RootEvent fEvent;
    bool fHasEvent;
    bool fHeatmap;
    bool fSurface;
    bool fHasHistogram;
    double fYaw;
    double fElevation;
    bool fDragging;
    QPoint fLastMousePosition;
    RootHistogram fHistogram;
};

#endif
