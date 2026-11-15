#include "rootviewerwindow_p.h"
#include "rootcanvas.h"

#include <QComboBox>
#include <QFileDialog>
#include <QPushButton>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QStatusBar>
#include <climits>
#include <memory>
#include <string>

void RootViewerWindow::OpenFile()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open ROOT event file"), QString(),
                                                       tr("ROOT files (*.root);;All files (*)"));
    if (path.isEmpty()) return;
    std::unique_ptr<GetDAQ::RootTreeReader> reader(new GetDAQ::RootTreeReader);
    std::string error;
    GetDAQ::RootEvent firstEvent;
    if (!reader->Open(path.toStdString(), &error) || !reader->Read(0, &firstEvent, &error)) {
        statusBar()->showMessage(tr("Cannot read legacy ROOT event tree: %1").arg(QString::fromLocal8Bit(error.c_str())));
        return;
    }
    if (reader->Entries() > INT_MAX) {
        statusBar()->showMessage(tr("This viewer supports at most 2,147,483,647 entries per file"));
        return;
    }
    fReader = std::move(reader);
    fEvent = std::move(firstEvent);
    fHasEvent = true;
    SetLegacyStatisticsEnabled(fEvent.format == GetDAQ::RootEvent::Format::GDataFrame);
    if (fEvent.format == GetDAQ::RootEvent::Format::T2KMatrix && fDisplay->currentIndex() >= 3)
        fDisplay->setCurrentIndex(0);
    fFrame->setRange(0, int(fReader->Entries() - 1));
    fFrame->setValue(0);
    fFrame->setEnabled(true);
    fPrevious->setEnabled(fReader->Entries() > 1);
    fNext->setEnabled(fReader->Entries() > 1);
    if (fDisplay->currentIndex() < 3) DisplayCurrentEvent();
    else BuildStatistics(fDisplay->currentIndex());
    if (fEvent.format == GetDAQ::RootEvent::Format::T2KMatrix)
        statusBar()->showMessage(tr("%1 — %2 T2K matrix entries; 272 channels × 512 buckets per entry")
                                 .arg(path).arg(fReader->Entries()));
    else
        statusBar()->showMessage(tr("%1 — %2 events; event %3, CoBo %4 / AsAd %5, %6 samples")
                                 .arg(path).arg(fReader->Entries()).arg(fEvent.event)
                                 .arg(fEvent.cobo).arg(fEvent.asad).arg(fEvent.sampleCount));
}

void RootViewerWindow::ReadEvent()
{
    if (!fReader) return;
    std::string error;
    if (!fReader->Read(fFrame->value(), &fEvent, &error)) {
        statusBar()->showMessage(tr("Cannot read ROOT event: %1").arg(QString::fromLocal8Bit(error.c_str())));
        return;
    }
    fHasEvent = true;
    if (fDisplay->currentIndex() < 3) DisplayCurrentEvent();
    if (fEvent.format == GetDAQ::RootEvent::Format::T2KMatrix)
        statusBar()->showMessage(tr("T2K matrix entry %1 — %2 channels × 512 buckets")
                                 .arg(fFrame->value()).arg(fEvent.channelCount));
    else
        statusBar()->showMessage(tr("Entry %1 — event %2, CoBo %3 / AsAd %4, %5 channels, %6 samples")
                                 .arg(fFrame->value()).arg(fEvent.event).arg(fEvent.cobo).arg(fEvent.asad)
                                 .arg(fEvent.channelCount).arg(fEvent.sampleCount));
}

void RootViewerWindow::DisplayChanged()
{
    if (!fHasEvent) return;
    const int mode = fDisplay->currentIndex();
    if (mode >= 3 && fEvent.format == GetDAQ::RootEvent::Format::T2KMatrix) {
        fDisplay->setCurrentIndex(0);
        statusBar()->showMessage(tr("Legacy statistics are defined for GDataFrame trees, not T2K matrices"));
        return;
    }
    if (mode < 3) {
        DisplayCurrentEvent();
        return;
    }
    BuildStatistics(mode);
}

void RootViewerWindow::SetLegacyStatisticsEnabled(bool enabled)
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>(fDisplay->model());
    if (!model) return;
    for (int index = 3; index < fDisplay->count(); ++index) {
        QStandardItem *item = model->item(index);
        if (item) item->setEnabled(enabled);
    }
}

void RootViewerWindow::DisplayCurrentEvent()
{
    const int mode = fDisplay->currentIndex();
    if (mode == 2) fCanvas->SetSurface(fEvent);
    else fCanvas->SetEvent(fEvent, mode == 1);
}
