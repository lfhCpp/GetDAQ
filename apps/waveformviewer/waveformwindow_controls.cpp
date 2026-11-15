#include <QAbstractItemView>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "waveformnavigation.h"
#include "waveformwindow_p.h"

namespace GetDAQ {
namespace detail {

void WaveformWindow::UpdateChannelButton()
    {
        fChannelsButton->setText(tr("Channels… (%1)").arg(fChannels.size()));
    }

void WaveformWindow::SelectChannels()
    {
        QDialog dialog(this);dialog.setObjectName("ChannelSelectionDialog");dialog.setWindowTitle(tr("Select AGET channels"));
        QVBoxLayout layout(&dialog);QListWidget list;list.setObjectName("WaveformChannelList");list.setSelectionMode(QAbstractItemView::NoSelection);
        for(int channel=0;channel<68;++channel){auto *item=new QListWidgetItem(tr("Channel %1").arg(channel),&list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(fChannels.contains(channel)?Qt::Checked:Qt::Unchecked);}
        layout.addWidget(&list);QHBoxLayout selection;QPushButton all(tr("All")),none(tr("None"));all.setObjectName("WaveformChannelsAll");none.setObjectName("WaveformChannelsNone");selection.addWidget(&all);selection.addWidget(&none);layout.addLayout(&selection);
        QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);
        connect(&all,&QPushButton::clicked,&dialog,[&list]{for(int i=0;i<list.count();++i)list.item(i)->setCheckState(Qt::Checked);});
        connect(&none,&QPushButton::clicked,&dialog,[&list]{for(int i=0;i<list.count();++i)list.item(i)->setCheckState(Qt::Unchecked);});
        connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        if(dialog.exec()!=QDialog::Accepted)return;
        fChannels.clear();for(int i=0;i<list.count();++i)if(list.item(i)->checkState()==Qt::Checked)fChannels.append(i);
        UpdateChannelButton();Refresh();
    }

void WaveformWindow::JumpToEvent() {
    if(!fHasFile || !fRawFile)return;
    bool valid=false;const auto event=fEvent->text().toULongLong(&valid);
    if(!valid || event>4294967295ULL){statusBar()->showMessage(tr("Event ID must be 0..4294967295."));return;}
    QVector<int> frames;
    for(int i=0;i<fRawFrames.size();++i)if(fRawFrames[i].event==event){
        if(frames.size()==4096){statusBar()->showMessage(tr("More than 4096 frames share this event ID; use the frame index to disambiguate."));return;}
        frames.append(i);
    }
    if(frames.isEmpty()){statusBar()->showMessage(tr("Event %1 was not found; displayed frame is unchanged.").arg(event));return;}
    fPlaybackTimer.stop();fAutoScan->setChecked(false);
    {const QSignalBlocker block(fEventFrames);fEventFrames->clear();
    for(int index:frames){const auto &frame=fRawFrames[index];fEventFrames->addItem(tr("Frame %1 · CoBo %2 / AsAd %3").arg(index).arg(frame.cobo).arg(frame.asad),index);}}
    fFrame->setValue(frames.first());Refresh();
}
void WaveformWindow::UpdateEventDisplay() {
    if(!fHasFile || !fRawFile || fFrame->value()>=fRawFrames.size())return;
    const auto &frame=fRawFrames[fFrame->value()];fEvent->setText(QString::number(frame.event));
    const QSignalBlocker block(fEventFrames);int index=fEventFrames->findData(fFrame->value());
    if(index<0){fEventFrames->clear();fEventFrames->addItem(tr("Frame %1 · CoBo %2 / AsAd %3").arg(fFrame->value()).arg(frame.cobo).arg(frame.asad),fFrame->value());index=0;}
    fEventFrames->setCurrentIndex(index);
}

void WaveformWindow::StepFrame(bool forward)
    {
        if (!fHasFile) return;
        const long long frameCount = static_cast<long long>(fFrame->maximum()) + 1LL;
        const long long frame = forward
            ? GetDAQ::NextWaveformFrameIndex(fFrame->value(), frameCount)
            : GetDAQ::PreviousWaveformFrameIndex(fFrame->value(), frameCount);
        if (frame >= 0) fFrame->setValue(static_cast<int>(frame));
    }

}
}
