#include "waveformscananalysis.h"

#include "grawreader.h"

#include <QHash>
#include <QMap>
#include <QVector>

#include <limits>

namespace GetDAQ {
QVector<PeakingScanSummary> AnalyzePeakingScan(const QVector<PeakingScanInput> &inputs,
                                               int cobo, int asad, int aget, int channel,
                                               QString *error)
{
    if (error) error->clear();
    if (inputs.isEmpty() || cobo < 0 || cobo > 255 || asad < 0 || asad > 3 ||
        aget < 0 || aget > 3 || channel < 0 || channel > 67) {
        if (error) *error = QStringLiteral("Provide input files and valid CoBo/AsAd/AGET/channel identifiers");
        return {};
    }

    QVector<PeakingScanSummary> summaries;
    QHash<int, int> summaryByIndex;
    int previousIndex = -1;
    for (const PeakingScanInput &input : inputs) {
        if (input.pointIndex < 0 || input.pointIndex > 15 || input.pointIndex < previousIndex || input.fileName.isEmpty()) {
            if (error) *error = QStringLiteral("Peaking-time indexes must be 0..15 in ascending order and each file must be named");
            return {};
        }
        previousIndex = input.pointIndex;
        if (!summaryByIndex.contains(input.pointIndex)) {
            PeakingScanSummary summary;
            summary.pointIndex = input.pointIndex;
            summaryByIndex.insert(input.pointIndex, summaries.size());
            summaries.append(summary);
        }
        PeakingScanSummary &summary = summaries[summaryByIndex.value(input.pointIndex)];
        const GrawIndexResult index = GrawFrameReader::BuildIndex(input.fileName);
        if (!index.error.isEmpty()) {
            if (error) *error = input.fileName + QStringLiteral(": ") + index.error;
            return {};
        }
        for (const GrawFrameRecord &frame : index.frames) {
            if (frame.cobo != cobo || frame.asad != asad) continue;
            ++summary.framesSeen;
            QVector<GrawSample> samples;
            QString readError;
            if (!GrawFrameReader::ReadChannel(input.fileName, frame, aget, channel, &samples, &readError)) {
                if (readError.contains(QStringLiteral("no selected channels have samples"), Qt::CaseInsensitive) ||
                    readError.contains(QStringLiteral("has no samples"), Qt::CaseInsensitive)) {
                    ++summary.channelMissing;
                    continue;
                }
                if (error) *error = input.fileName + QStringLiteral(": ") + readError;
                return {};
            }
            ++summary.channelPresent;
            const WaveformAnalysisResult metrics = AnalyzeWaveform(samples);
            if (!metrics.valid) {
                ++summary.invalidWaveforms;
                continue;
            }
            ++summary.validWaveforms;
            summary.meanPeakAdc += metrics.peakValue;
            summary.meanPeakingBuckets += metrics.peakingBuckets;
            summary.meanFwhmBuckets += metrics.fwhmBuckets;
        }
    }
    for (PeakingScanSummary &summary : summaries) {
        if (summary.validWaveforms == 0) continue;
        const double count = static_cast<double>(summary.validWaveforms);
        summary.meanPeakAdc /= count;
        summary.meanPeakingBuckets /= count;
        summary.meanFwhmBuckets /= count;
    }
    return summaries;
}

QVector<PulserResponseSummary> AnalyzePulserResponse(const QVector<PulserResponseInput> &inputs,
                                                     int cobo, int asad, int aget,
                                                     QString *error)
{
    if (error) error->clear();
    if (inputs.isEmpty() || cobo < 0 || cobo > 255 || asad < 0 || asad > 3 ||
        aget < 0 || aget > 3) {
        if (error) *error = QStringLiteral("Provide input files and valid CoBo/AsAd/AGET identifiers");
        return {};
    }

    QVector<PulserResponseSummary> summaries;
    QMap<QPair<int, int>, int> summaryByPoint;
    QPair<int, int> previousPoint(-1, std::numeric_limits<int>::min());
    bool havePreviousPoint = false;
    for (const PulserResponseInput &input : inputs) {
        const QPair<int, int> point(input.channel, input.pulserMillivolts);
        if (input.channel < 0 || input.channel > 67 || input.fileName.isEmpty() ||
            (havePreviousPoint && point < previousPoint)) {
            if (error) *error = QStringLiteral("Channel/amplitude inputs must be ordered by channel and ascending amplitude; each file must be named");
            return {};
        }
        previousPoint = point;
        havePreviousPoint = true;
        if (!summaryByPoint.contains(point)) {
            PulserResponseSummary summary;
            summary.channel = input.channel;
            summary.pulserMillivolts = input.pulserMillivolts;
            summaryByPoint.insert(point, summaries.size());
            summaries.append(summary);
        }
        PulserResponseSummary &summary = summaries[summaryByPoint.value(point)];
        const GrawIndexResult index = GrawFrameReader::BuildIndex(input.fileName);
        if (!index.error.isEmpty()) {
            if (error) *error = input.fileName + QStringLiteral(": ") + index.error;
            return {};
        }
        for (const GrawFrameRecord &frame : index.frames) {
            if (frame.cobo != cobo || frame.asad != asad) continue;
            ++summary.framesSeen;
            QVector<GrawSample> samples;
            QString readError;
            if (!GrawFrameReader::ReadChannel(input.fileName, frame, aget, input.channel, &samples, &readError)) {
                if (readError.contains(QStringLiteral("no selected channels have samples"), Qt::CaseInsensitive) ||
                    readError.contains(QStringLiteral("has no samples"), Qt::CaseInsensitive)) {
                    ++summary.channelMissing;
                    continue;
                }
                if (error) *error = input.fileName + QStringLiteral(": ") + readError;
                return {};
            }
            ++summary.channelPresent;
            const WaveformAnalysisResult metrics = AnalyzeWaveform(samples);
            if (!metrics.valid) {
                ++summary.invalidWaveforms;
                continue;
            }
            ++summary.validWaveforms;
            summary.meanPeakAdc += metrics.peakValue;
            summary.meanPeakingBuckets += metrics.peakingBuckets;
            summary.meanFwhmBuckets += metrics.fwhmBuckets;
        }
    }
    for (PulserResponseSummary &summary : summaries) {
        if (summary.validWaveforms == 0) continue;
        const double count = static_cast<double>(summary.validWaveforms);
        summary.meanPeakAdc /= count;
        summary.meanPeakingBuckets /= count;
        summary.meanFwhmBuckets /= count;
    }
    return summaries;
}
}
