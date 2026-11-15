#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>

#include "waveformanalysis.h"
#include "waveformcanvas.h"
#include "waveformframe.h"
#include "waveformwindow_p.h"

namespace GetDAQ {
namespace detail {

void WaveformWindow::Refresh()
    {
        if (fFileName.isEmpty()) return;
        UpdateEventDisplay();
        fCanvas->Clear();
        fExport->setEnabled(false);
        fMetrics->setText(tr("Pulse metrics: not calculated"));
        fExportMetadata = GetDAQ::WaveformExportMetadata();
        fExportMetadata.sourceFile = fFileName;
        fExportMetadata.frame = QString::number(fFrame->value());
        fExportMetadata.aget = fAget->value();
        fExportMetadata.channel = fChannels.size() == 1 ? fChannels.first() : -1;
        if(fChannels.isEmpty()){fMetrics->setText(tr("No channels selected. Use Channels… to choose channels."));return;}
        GetDAQ::WaveformFrameData frameData;
        QString error;
        if (fRawFile) {
            const GetDAQ::GrawFrameRecord &frame = fRawFrames.at(fFrame->value());
            fExportMetadata.cobo = QString::number(frame.cobo);
            fExportMetadata.asad = QString::number(frame.asad);
            if (!GetDAQ::ReadGrawWaveformFrame(fFileName, frame, fAget->value(), fChannels, &frameData, &error)) {
                statusBar()->showMessage(error);
                return;
            }
            fCanvas->SetTraces(frameData.traces, fAget->value());
            fExport->setEnabled(true);
            if (fChannels.size() == 1) ShowMetrics(frameData.metricSamples);
            else fMetrics->setText(tr("Pulse metrics: select exactly one channel to calculate metrics"));
            int totalSamples = 0;
            for (const GetDAQ::WaveformPlotTrace &trace : frameData.traces) totalSamples += trace.samples.size();
            statusBar()->showMessage(tr("Raw CoBo frame %1: %2 channel traces / %3 samples; event %4; CoBo %5 / AsAd %6")
                .arg(fFrame->value()).arg(frameData.traces.size()).arg(totalSamples).arg(frame.event).arg(frame.cobo).arg(frame.asad));
            return;
        }
        if (!GetDAQ::ReadGmatWaveformFrame(fFileName, fFrame->value(), fAget->value(), fChannels, &frameData, &error)) {
            statusBar()->showMessage(error);
            return;
        }
        fCanvas->SetTraces(frameData.traces, fAget->value());
        fExport->setEnabled(true);
        if (fChannels.size() > 1) {
            fMetrics->setText(tr("Pulse metrics: select exactly one channel to calculate metrics"));
            return;
        }
        if (!frameData.integerAdcSamples) {
            fMetrics->setText(tr("Pulse metrics: .gmat values are not integer CoBo ADC samples"));
            return;
        }
        ShowMetrics(frameData.metricSamples);
    }

void WaveformWindow::ExportWaveform()
    {
        if (fCanvas->Traces().isEmpty()) return;
        const QString baseName = QFileInfo(fFileName).completeBaseName() +
            QStringLiteral("_frame_%1_aget_%2_channels_%3")
                .arg(fFrame->value()).arg(fAget->value()).arg(fChannels.size());
        QString selectedFilter = tr("PNG image (*.png)");
        QString path = QFileDialog::getSaveFileName(this, tr("Export selected waveform"), baseName + ".png",
            tr("PNG image (*.png);;CSV sample data (*.csv)"), &selectedFilter);
        if (path.isEmpty()) return;
        const QString existingSuffix = QFileInfo(path).suffix().toLower();
        if (!existingSuffix.isEmpty() && existingSuffix != QStringLiteral("csv") && existingSuffix != QStringLiteral("png")) {
            statusBar()->showMessage(tr("Choose a PNG or CSV filename"));
            return;
        }
        const bool csv = existingSuffix == QStringLiteral("csv") ||
            (existingSuffix.isEmpty() && selectedFilter.startsWith(tr("CSV sample data")));
        const QString suffix = csv ? QStringLiteral("csv") : QStringLiteral("png");
        if (QFileInfo(path).suffix().isEmpty()) {
            path += QLatin1Char('.');
            path += suffix;
        }
        QString error;
        const bool saved = csv
            ? GetDAQ::WriteWaveformCsv(path, fCanvas->Traces(), fExportMetadata, &error)
            : fCanvas->SavePng(path, &error);
        statusBar()->showMessage(saved ? tr("Waveform exported to %1").arg(path)
                                       : tr("Cannot export waveform: %1").arg(error));
    }

void WaveformWindow::ShowMetrics(const QVector<GetDAQ::GrawSample> &samples)
    {
        const GetDAQ::WaveformAnalysisResult result = GetDAQ::AnalyzeWaveform(samples);
        if (!result.valid) {
            fMetrics->setText(tr("Pulse metrics unavailable: %1").arg(result.error));
            return;
        }
        fMetrics->setText(tr("Peak: %1 ADC at bucket %2 | FWHM: %3 buckets (%4 ns at legacy 100 MHz) | "
                             "peaking time: %5 buckets (%6 ns at legacy 100 MHz)")
            .arg(result.peakValue).arg(result.peakBucket).arg(result.fwhmBuckets)
            .arg(unsigned(result.fwhmBuckets) * 10u).arg(result.peakingBuckets)
            .arg(unsigned(result.peakingBuckets) * 10u));
    }

}
}
