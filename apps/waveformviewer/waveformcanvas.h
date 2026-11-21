#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMCANVAS_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMCANVAS_H

#include "waveformexport.h"

#include <QPoint>
#include <QWidget>

class QMouseEvent;
class QWheelEvent;
class QPaintEvent;
class QPainter;
class QRectF;
class QColor;

namespace GetDAQ {

// Offline waveform rendering widget shared by the integrated and standalone viewers.
class WaveformCanvas : public QWidget {
public:
    WaveformCanvas();

    void SetTraces(const QVector<WaveformPlotTrace> &traces, int aget);
    void Clear();
    void SetSurface(bool enabled);
    const QVector<WaveformPlotTrace> &Traces() const;
    bool SavePng(const QString &path, QString *error);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void UpdateSurfaceSize();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QColor ChannelColor(int channel) const;
    QPointF ProjectSurfacePoint(double bucket, double channel, double normalizedValue,
                                const QRectF &plot, double *depth) const;
    void DrawSurface(QPainter &painter);

    QVector<WaveformPlotTrace> fTraces;
    int fAget;
    bool fHasData;
    bool fSurface;
    double fDepthMinimum=0, fDepthMaximum=67;
    int fChannelGap=16;
    double fYaw;
    double fElevation;
    bool fDragging;
    QPoint fLastMousePosition;
};

}

#endif
