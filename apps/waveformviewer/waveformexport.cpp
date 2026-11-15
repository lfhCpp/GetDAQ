#include "waveformexport.h"

#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>
#include <QStringList>
#include <QtGlobal>

namespace {
QByteArray CsvField(const QString &value)
{
    QByteArray field = value.toUtf8();
    if (field.contains(',') || field.contains('"') || field.contains('\r') || field.contains('\n')) {
        field.replace("\"", "\"\"");
        field.prepend('"');
        field.append('"');
    }
    return field;
}

bool WriteAll(QSaveFile &file, const QByteArray &bytes)
{
    qint64 written = 0;
    while (written < bytes.size()) {
        const qint64 count = file.write(bytes.constData() + written, bytes.size() - written);
        if (count <= 0) return false;
        written += count;
    }
    return true;
}
}

namespace GetDAQ {
bool WriteWaveformCsv(const QString &path, const QVector<WaveformPlotTrace> &traces,
                      const WaveformExportMetadata &metadata, QString *error)
{
    if (!error) return false;
    error->clear();
    if (path.isEmpty() || traces.isEmpty()) {
        *error = QStringLiteral("An output path and at least one waveform trace are required");
        return false;
    }
    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        *error = output.errorString();
        return false;
    }
    QByteArray bytes("source,frame,cobo,asad,aget,channel,bucket,adc\r\n");
    for (const WaveformPlotTrace &trace : traces) {
        if (trace.channel < 0 || trace.channel >= 68 || trace.samples.isEmpty()) {
            output.cancelWriting();
            *error = QStringLiteral("A waveform trace has an invalid channel or no samples");
            return false;
        }
        for (const QPointF &sample : trace.samples) {
            if (!qIsFinite(sample.x()) || !qIsFinite(sample.y())) {
                output.cancelWriting();
                *error = QStringLiteral("Cannot export a non-finite waveform sample");
                return false;
            }
            const QStringList fields = {metadata.sourceFile, metadata.frame, metadata.cobo, metadata.asad,
                QString::number(metadata.aget), QString::number(trace.channel),
                QString::number(sample.x(), 'g', 17), QString::number(sample.y(), 'g', 17)};
            for (int i = 0; i < fields.size(); ++i) {
                if (i) bytes.append(',');
                bytes.append(CsvField(fields.at(i)));
            }
            bytes.append("\r\n");
        }
    }
    if (!WriteAll(output, bytes)) {
        *error = output.errorString();
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        *error = output.errorString();
        return false;
    }
    return true;
}

bool WriteWaveformCsv(const QString &path, const QVector<QPointF> &samples,
                      const WaveformExportMetadata &metadata, QString *error)
{
    if (!error) return false;
    error->clear();
    if (path.isEmpty() || samples.isEmpty()) {
        *error = QStringLiteral("An output path and at least one sample are required");
        return false;
    }

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        *error = output.errorString();
        return false;
    }

    QByteArray bytes("source,frame,cobo,asad,aget,channel,bucket,adc\r\n");
    for (const QPointF &sample : samples) {
        if (!qIsFinite(sample.x()) || !qIsFinite(sample.y())) {
            output.cancelWriting();
            *error = QStringLiteral("Cannot export a non-finite waveform sample");
            return false;
        }
        const QStringList fields = {
            metadata.sourceFile, metadata.frame, metadata.cobo, metadata.asad,
            QString::number(metadata.aget), QString::number(metadata.channel),
            QString::number(sample.x(), 'g', 17), QString::number(sample.y(), 'g', 17)
        };
        for (int i = 0; i < fields.size(); ++i) {
            if (i) bytes.append(',');
            bytes.append(CsvField(fields.at(i)));
        }
        bytes.append("\r\n");
    }

    if (!WriteAll(output, bytes)) {
        *error = output.errorString();
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        *error = output.errorString();
        return false;
    }
    return true;
}

bool WriteWidgetPng(const QString &path, QWidget *widget, QString *error)
{
    if (!error) return false;
    error->clear();
    if (!widget || path.isEmpty() || widget->width() <= 0 || widget->height() <= 0) {
        *error = QStringLiteral("A sized plot widget and output path are required");
        return false;
    }

    const QSize imageSize = widget->size() * 2;
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        *error = QStringLiteral("Cannot allocate the plot image");
        return false;
    }
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.scale(2.0, 2.0);
    widget->render(&painter);
    painter.end();

    QSaveFile output(path);
    if (!output.open(QIODevice::WriteOnly)) {
        *error = output.errorString();
        return false;
    }
    QImageWriter writer(&output, "png");
    if (!writer.write(image)) {
        *error = writer.errorString();
        output.cancelWriting();
        return false;
    }
    if (!output.commit()) {
        *error = output.errorString();
        return false;
    }
    return true;
}
}
