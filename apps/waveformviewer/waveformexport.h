#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMEXPORT_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMEXPORT_H

#include <QPointF>
#include <QString>
#include <QVector>
#include <QWidget>

namespace GetDAQ {

struct WaveformExportMetadata {
    QString sourceFile;
    QString frame;
    QString cobo;
    QString asad;
    int aget = 0;
    int channel = 0;
};

struct WaveformPlotTrace {
    int channel = 0;
    QVector<QPointF> samples;
};

// Writes the currently displayed samples as an RFC 4180-style CSV file.
bool WriteWaveformCsv(const QString &path, const QVector<QPointF> &samples,
                      const WaveformExportMetadata &metadata, QString *error);
bool WriteWaveformCsv(const QString &path, const QVector<WaveformPlotTrace> &traces,
                      const WaveformExportMetadata &metadata, QString *error);
bool WriteWidgetPng(const QString &path, QWidget *widget, QString *error);

}

#endif
