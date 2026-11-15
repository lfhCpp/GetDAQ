#include "waveformviewer_tests_p.h"

#include <QtTest>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPointF>
#include <QTemporaryDir>
#include <QWidget>
#include <atomic>
#include <limits>
#include "../apps/waveformviewer/grawreader.h"
#include "../apps/waveformviewer/waveformanalysis.h"
#include "../apps/waveformviewer/waveformscananalysis.h"
#include "../apps/waveformviewer/waveformexport.h"
#include "../apps/waveformviewer/waveformnavigation.h"

namespace GetDAQTestInternal {

void RunWaveformViewerIndexesRawAndLayeredFrames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QByteArray frameBytes = MakeType1Frame();
    const QString rawPath = directory.filePath("ordinary.graw");
    QVERIFY(WriteWaveformFixture(rawPath, frameBytes));
    GetDAQ::GrawIndexResult index = GetDAQ::GrawFrameReader::BuildIndex(rawPath);
    QVERIFY2(index.error.isEmpty(), qPrintable(index.error));
    QCOMPARE(index.frames.size(), 1);
    QCOMPARE(index.frames.first().event, quint32(1234));
    QCOMPARE(index.frames.first().cobo, quint8(2));
    QCOMPARE(index.frames.first().asad, quint8(1));
    QVector<GetDAQ::GrawSample> samples;
    QString error;
    QVERIFY2(GetDAQ::GrawFrameReader::ReadChannel(rawPath, index.frames.first(), 2, 7, &samples, &error), qPrintable(error));
    QCOMPARE(samples.size(), 512);
    QCOMPARE(samples.first().bucket, quint16(100));
    QCOMPARE(samples.first().value, quint16(0));
    QCOMPARE(samples.last().bucket, quint16(99));
    QCOMPARE(samples.last().value, quint16(511));
    QVector<GetDAQ::GrawChannelSamples> traces;
    QVERIFY2(GetDAQ::GrawFrameReader::ReadChannels(rawPath, index.frames.first(), 2,
        QVector<int>() << 7 << 8 << 9, &traces, &error), qPrintable(error));
    QCOMPARE(traces.size(), 2); // channel 9 is absent in this sparse CoBo frame
    QCOMPARE(traces.at(0).channel, 7);
    QCOMPARE(traces.at(0).samples.size(), 512);
    QCOMPARE(traces.at(1).channel, 8);
    QCOMPARE(traces.at(1).samples.first().value, quint16(1000));
    QVERIFY(!GetDAQ::GrawFrameReader::ReadChannels(rawPath, index.frames.first(), 2,
        QVector<int>() << 7 << 7, &traces, &error));
    QVERIFY(error.contains(QStringLiteral("duplicate")));
    QVERIFY(traces.isEmpty());
    QVERIFY(!GetDAQ::GrawFrameReader::ReadChannels(rawPath, index.frames.first(), 2,
        QVector<int>(), &traces, &error));
    QVERIFY(traces.isEmpty());
    QFile changed(rawPath);
    QVERIFY(changed.open(QIODevice::ReadWrite));
    QVERIFY(changed.seek(22));
    QVERIFY(changed.write("\0\0\x04\xd3", 4) == 4);
    changed.close();
    QVERIFY(!GetDAQ::GrawFrameReader::ReadChannel(rawPath, index.frames.first(), 2, 7, &samples, &error));
    QVERIFY(error.contains(QStringLiteral("changed after indexing")));

    QByteArray layered(256 + frameBytes.size(), '\0');
    layered[0] = char(8); Put24BigEndian(&layered, 1, layered.size() / 256);
    Put16BigEndian(&layered, 5, 0xff01); layered[7] = char(1);
    Put16BigEndian(&layered, 8, 1); Put16BigEndian(&layered, 10, 0);
    Put32BigEndian(&layered, 12, 1); Put32BigEndian(&layered, 16, 1234);
    layered.replace(256, frameBytes.size(), frameBytes);
    const QString layeredPath = directory.filePath("layered.graw");
    QVERIFY(WriteWaveformFixture(layeredPath, layered));
    index = GetDAQ::GrawFrameReader::BuildIndex(layeredPath);
    QVERIFY2(index.error.isEmpty(), qPrintable(index.error));
    QCOMPARE(index.frames.size(), 1);
    QCOMPARE(index.frames.first().offset, qint64(256));
    QVERIFY2(GetDAQ::GrawFrameReader::ReadChannel(layeredPath, index.frames.first(), 2, 7, &samples, &error), qPrintable(error));
    QCOMPARE(samples.size(), 512);
}

void RunWaveformViewerDecodesCompactCoboFrames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("compact.graw");
    QVERIFY(WriteWaveformFixture(path, MakeType2Frame()));
    const GetDAQ::GrawIndexResult index = GetDAQ::GrawFrameReader::BuildIndex(path);
    QVERIFY2(index.error.isEmpty(), qPrintable(index.error));
    QCOMPARE(index.frames.size(), 1);
    QVector<GetDAQ::GrawSample> samples;
    QString error;
    QVERIFY2(GetDAQ::GrawFrameReader::ReadChannel(path, index.frames.first(), 2, 7, &samples, &error), qPrintable(error));
    QCOMPARE(samples.size(), 512);
    QCOMPARE(samples.first().bucket, quint16(0));
    QCOMPARE(samples.first().value, quint16((2 * 100 + 7 * 2) & 0xfff));
    QCOMPARE(samples.last().bucket, quint16(511));
    QCOMPARE(samples.last().value, quint16((2 * 100 + 7 * 2 + 511) & 0xfff));
    QVector<GetDAQ::GrawChannelSamples> traces;
    QVERIFY2(GetDAQ::GrawFrameReader::ReadChannels(path, index.frames.first(), 2,
        QVector<int>() << 8 << 7, &traces, &error), qPrintable(error));
    QCOMPARE(traces.size(), 2);
    QCOMPARE(traces.at(0).channel, 8);
    QCOMPARE(traces.at(0).samples.size(), 512);
    QCOMPARE(traces.at(1).channel, 7);
    QCOMPARE(traces.at(1).samples.first().value, quint16((2 * 100 + 7 * 2) & 0xfff));
}

void RunWaveformViewerRejectsTruncatedMfmInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("truncated.graw");
    QVERIFY(WriteWaveformFixture(path, QByteArray(8, '\0')));
    const GetDAQ::GrawIndexResult index = GetDAQ::GrawFrameReader::BuildIndex(path);
    QVERIFY(!index.error.isEmpty());
    QVERIFY(index.frames.isEmpty());
    const std::atomic<bool> cancelled(true);
    const GetDAQ::GrawIndexResult cancelledIndex = GetDAQ::GrawFrameReader::BuildIndex(path, &cancelled);
    QCOMPARE(cancelledIndex.error, QStringLiteral("MFM frame index cancelled"));
}

void RunWaveformViewerComputesLegacyPulseMetrics()
{
    QVector<GetDAQ::GrawSample> dense;
    const quint16 values[] = {0, 10, 50, 80, 100, 80, 50, 49, 10, 0};
    for (int i = 0; i < 10; ++i) {
        GetDAQ::GrawSample sample; sample.bucket = quint16(i); sample.value = values[i];
        dense.append(sample);
    }
    GetDAQ::WaveformAnalysisResult result = GetDAQ::AnalyzeWaveform(dense);
    QVERIFY(result.valid);
    QCOMPARE(result.peakBucket, quint16(4));
    QCOMPARE(result.peakValue, quint16(100));
    QCOMPARE(result.peakingBuckets, quint16(3));
    QCOMPARE(result.fwhmBuckets, quint16(5));

    QVector<GetDAQ::GrawSample> oddPeak;
    const quint16 oddValues[] = {0, 1, 8, 12, 17, 12, 8, 7, 0};
    for (int i = 0; i < 9; ++i) {
        GetDAQ::GrawSample sample; sample.bucket = quint16(i); sample.value = oddValues[i];
        oddPeak.append(sample);
    }
    result = GetDAQ::AnalyzeWaveform(oddPeak);
    QVERIFY(result.valid);
    QCOMPARE(result.peakingBuckets, quint16(3));
    QCOMPARE(result.fwhmBuckets, quint16(5)); // legacy uint16 threshold truncates 8.5 to 8

    QVector<GetDAQ::GrawSample> sparse;
    const quint16 buckets[] = {12, 2, 9, 5, 4};
    const quint16 sparseValues[] = {0, 0, 40, 100, 50};
    for (int i = 0; i < 5; ++i) {
        GetDAQ::GrawSample sample; sample.bucket = buckets[i]; sample.value = sparseValues[i];
        sparse.append(sample);
    }
    result = GetDAQ::AnalyzeWaveform(sparse);
    QVERIFY(result.valid);
    QCOMPARE(result.peakBucket, quint16(5));
    QCOMPARE(result.peakingBuckets, quint16(1));
    QCOMPARE(result.fwhmBuckets, quint16(5));

    sparse[0].bucket = 4;
    QVERIFY(!GetDAQ::AnalyzeWaveform(sparse).valid);
    sparse.clear();
    GetDAQ::GrawSample sample; sample.bucket = 1; sample.value = 0; sparse.append(sample);
    sample.bucket = 2; sample.value = 0; sparse.append(sample);
    QVERIFY(!GetDAQ::AnalyzeWaveform(sparse).valid);
    QVERIFY(!GetDAQ::AnalyzeWaveform(dense, 0.0, 1.1, 0.5).valid);
}

void RunWaveformViewerSummarizesPeakingTimeScan()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("pulse.graw"));
    QVERIFY(WriteWaveformFixture(path, MakePulseType1Frame()));
    QVector<GetDAQ::PeakingScanInput> inputs;
    GetDAQ::PeakingScanInput input; input.pointIndex = 3; input.fileName = path; inputs.append(input);
    input.pointIndex = 4; inputs.append(input);
    QString error;
    const QVector<GetDAQ::PeakingScanSummary> summaries =
        GetDAQ::AnalyzePeakingScan(inputs, 2, 1, 2, 7, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(summaries.size(), 2);
    for (const GetDAQ::PeakingScanSummary &summary : summaries) {
        QCOMPARE(summary.framesSeen, quint64(1));
        QCOMPARE(summary.channelPresent, quint64(1));
        QCOMPARE(summary.validWaveforms, quint64(1));
        QCOMPARE(summary.invalidWaveforms, quint64(0));
        QCOMPARE(summary.meanPeakAdc, 100.0);
        QCOMPARE(summary.meanPeakingBuckets, 3.0);
        QCOMPARE(summary.meanFwhmBuckets, 5.0);
    }
    input.pointIndex = 2; inputs.append(input);
    QVERIFY(GetDAQ::AnalyzePeakingScan(inputs, 2, 1, 2, 7, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void RunWaveformViewerSummarizesPulserResponse()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("pulse.graw"));
    QVERIFY(WriteWaveformFixture(path, MakePulseType1Frame()));
    QVector<GetDAQ::PulserResponseInput> inputs;
    GetDAQ::PulserResponseInput input; input.channel = 7; input.pulserMillivolts = 100; input.fileName = path;
    inputs.append(input); inputs.append(input);
    input.pulserMillivolts = 200; inputs.append(input);
    input.channel = 8; input.pulserMillivolts = 100; inputs.append(input);
    QString error;
    const QVector<GetDAQ::PulserResponseSummary> summaries =
        GetDAQ::AnalyzePulserResponse(inputs, 2, 1, 2, &error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QCOMPARE(summaries.size(), 3);
    QCOMPARE(summaries[0].channel, 7);
    QCOMPARE(summaries[0].pulserMillivolts, 100);
    QCOMPARE(summaries[0].framesSeen, quint64(2));
    QCOMPARE(summaries[0].validWaveforms, quint64(2));
    QCOMPARE(summaries[0].meanPeakAdc, 100.0);
    QCOMPARE(summaries[1].pulserMillivolts, 200);
    QCOMPARE(summaries[1].validWaveforms, quint64(1));
    QCOMPARE(summaries[2].channel, 8);
    QCOMPARE(summaries[2].channelMissing, quint64(1));
    QCOMPARE(summaries[2].validWaveforms, quint64(0));
    input.channel = 7; input.pulserMillivolts = 50; inputs.append(input);
    QVERIFY(GetDAQ::AnalyzePulserResponse(inputs, 2, 1, 2, &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void RunWaveformViewerNavigatesAndWrapsFrames()
{
    QCOMPARE(GetDAQ::NextWaveformFrameIndex(0, 4), 1);
    QCOMPARE(GetDAQ::NextWaveformFrameIndex(3, 4), 0);
    QCOMPARE(GetDAQ::PreviousWaveformFrameIndex(2, 4), 1);
    QCOMPARE(GetDAQ::PreviousWaveformFrameIndex(0, 4), 3);
    QCOMPARE(GetDAQ::NextWaveformFrameIndex(0, 1), 0);
    QCOMPARE(GetDAQ::PreviousWaveformFrameIndex(0, 1), 0);
    QCOMPARE(GetDAQ::NextWaveformFrameIndex(0, 0), -1);
    QCOMPARE(GetDAQ::PreviousWaveformFrameIndex(0, 0), -1);
    QCOMPARE(GetDAQ::NextWaveformFrameIndex(-1, 4), -1);
    QCOMPARE(GetDAQ::PreviousWaveformFrameIndex(4, 4), -1);
}

void RunWaveformViewerExportsSelectedSamplesAsCsv()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("selected.csv");
    QVector<QPointF> samples;
    samples.append(QPointF(9, 1.5));
    samples.append(QPointF(2, 4095));
    samples.append(QPointF(0.1, 0.1));
    GetDAQ::WaveformExportMetadata metadata;
    metadata.sourceFile = QStringLiteral("run,\"east\".graw");
    metadata.frame = QStringLiteral("7");
    metadata.cobo = QStringLiteral("4");
    metadata.asad = QStringLiteral("2");
    metadata.aget = 2;
    metadata.channel = 1;
    QString error;
    QVERIFY2(GetDAQ::WriteWaveformCsv(path, samples, metadata, &error), qPrintable(error));
    QFile output(path);
    QVERIFY(output.open(QIODevice::ReadOnly));
    QCOMPARE(output.readAll(), QByteArray(
        "source,frame,cobo,asad,aget,channel,bucket,adc\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,1,9,1.5\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,1,2,4095\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,1,0.10000000000000001,0.10000000000000001\r\n"));
    output.close();

    const QString multiPath = directory.filePath("channels.csv");
    QVector<GetDAQ::WaveformPlotTrace> traces;
    GetDAQ::WaveformPlotTrace first, second;
    first.channel = 7;
    first.samples << QPointF(0, 12) << QPointF(1, 13);
    second.channel = 8;
    second.samples << QPointF(0, 22);
    traces << first << second;
    QVERIFY2(GetDAQ::WriteWaveformCsv(multiPath, traces, metadata, &error), qPrintable(error));
    QFile multiOutput(multiPath);
    QVERIFY(multiOutput.open(QIODevice::ReadOnly));
    QCOMPARE(multiOutput.readAll(), QByteArray(
        "source,frame,cobo,asad,aget,channel,bucket,adc\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,7,0,12\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,7,1,13\r\n"
        "\"run,\"\"east\"\".graw\",7,4,2,2,8,0,22\r\n"));
    multiOutput.close();

    const QString protectedPath = directory.filePath("preserved.csv");
    QFile protectedFile(protectedPath);
    QVERIFY(protectedFile.open(QIODevice::WriteOnly));
    QCOMPARE(protectedFile.write("prior"), qint64(5));
    protectedFile.close();
    samples[1].setY(std::numeric_limits<double>::quiet_NaN());
    QVERIFY(!GetDAQ::WriteWaveformCsv(protectedPath, samples, metadata, &error));
    QVERIFY(protectedFile.open(QIODevice::ReadOnly));
    QCOMPARE(protectedFile.readAll(), QByteArray("prior"));

    QWidget plot;
    plot.resize(80, 40);
    const QString imagePath = directory.filePath("selected.png");
    QVERIFY2(GetDAQ::WriteWidgetPng(imagePath, &plot, &error), qPrintable(error));
    QImage image;
    QVERIFY(image.load(imagePath));
    QCOMPARE(image.size(), QSize(160, 80));
}


} // namespace GetDAQTestInternal
