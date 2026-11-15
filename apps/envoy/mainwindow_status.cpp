#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgetplot2d.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include "configurationreadbackwindow.h"
#include "plantransitionwindow.h"
#include "eccprotocol.h"
#include <QAction>
#include <QColor>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <stdexcept>

void MainWindow::ShowStatusSlot(QVector<NodeStatus> status)
{
    fObservedEccState=-1;
    if(fConfig.nativeEcc && !status.isEmpty()) {
        int observed=status[0].state;bool consistent=true;
        for(const auto &s:status)consistent=consistent && s.eccOnline && !s.nativeHardwareOutcomeUnknown && s.transition==0 && s.state==observed;
        if(consistent)fObservedEccState=observed;
    }
    QMap<int,QString> projectedLifecycles;
    QMap<QString,GetDAQ::EccControlMaintenanceTarget> coboLifecycle;
    if(fConfig.nativeEcc && !fCoBoTargets.isEmpty())try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fCoBoTargets.toStdString());
        for(const auto &target:targets.cobos)coboLifecycle.insert(QString::fromStdString(target.id),target);
    }catch(...) {coboLifecycle.clear();}
    const auto paintEccState = [this](int row, const NodeStatus &node) {
        auto *item = ui->StatusTable->item(row, 1);
        if (!item) return;
        QColor foreground("#617383");
        if (!node.eccOnline) foreground = QColor("#b53840");
        else if (node.transition != 0) foreground = QColor("#996910");
        else if (node.state == 4 || node.state == 5) foreground = QColor("#087f5b");
        else if (node.state == 6) foreground = QColor("#24658d");
        else if (node.state < 0 || node.state > 6) foreground = QColor("#b53840");
        item->setForeground(foreground);
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(node.eccOnline
            ? tr("ECC system state: %1 · transition: %2").arg(StateName(node.state), TransitionName(node.transition))
            : tr("ECC state is offline or stale; displayed state is not current."));
    };
    const auto paintTransition = [this](int row, const NodeStatus &node) {
        auto *item = ui->StatusTable->item(row, 2);
        if (!item) return;
        QColor foreground("#617383");
        if (!node.eccOnline || node.transition < 0 || node.transition > 9) {
            foreground = QColor("#b53840");
        } else if (node.transition != 0) {
            foreground = QColor("#996910");
        }
        item->setForeground(foreground);
        item->setTextAlignment(Qt::AlignCenter);
        item->setToolTip(!node.eccOnline
            ? tr("Transition is offline or stale; displayed value is not current.")
            : fConfig.nativeEcc
                ? tr("Transition of the whole ECC system; repeated for each configured data source.")
                : tr("Transition of this selected ECC node."));
    };
    fHardwareOutcomeUnknown=false;for(const auto &row:status) fHardwareOutcomeUnknown=fHardwareOutcomeUnknown || row.nativeHardwareOutcomeUnknown;
    fReceiverAbort->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fFailureBreakup->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fReceiverRetirement->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fSourceRetirement->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fSourceRecovery->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fAcquisitionUnprepare->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fAcquisitionRecovery->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fAlarmAcquisitionRecovery->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fDescriptionRecovery->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fPreparationRecovery->setEnabled(fConfig.nativeEcc && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fAlarmPreparationRecovery->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && fConnected && !fBusy && fHardwareOutcomeUnknown);
    fStoppedAlarmRelease->setEnabled(fConfig.nativeEcc && fConfig.nativeClient.alarmStatusEnabled && fConnected && !fBusy && !fHardwareOutcomeUnknown);
    fRemoteRunning = false;
    for (const NodeStatus &s : status) if (s.state == 5 || s.state == 6) fRemoteRunning = true;
    for (const NodeStatus &s : status) for (int row = 0; row < ui->StatusTable->rowCount(); ++row) {
        if (ui->StatusTable->item(row, 0)->data(Qt::UserRole).toInt() != s.id) continue;
        const bool mutant = fConfig.nodes[row].mutant;
        if (fConfig.nativeEcc) {
            const auto &node=fConfig.nodes[row];
            const GetDAQ::EccControlMaintenanceTarget *board=nullptr;
            if(!mutant) {
                const auto bySender=coboLifecycle.constFind(node.nativeSenderId);
                if(bySender!=coboLifecycle.constEnd())board=&bySender.value();
                else {const auto byId=coboLifecycle.constFind(QString::number(node.id));if(byId!=coboLifecycle.constEnd())board=&byId.value();}
            }
            QString lifecycle=s.eccOnline ? StateName(s.state) : "Offline / stale";
            if(board) {
                const QMap<QString,QString> labels={{"uninitialized",tr("Uninitialized")},{"described",tr("Described")},{"prepared",tr("Prepared")},
                    {"configured",tr("Configured")},{"running",tr("Running")},{"paused",tr("Paused")},{"transitioning",tr("Transitioning")},{"unknown",tr("UNKNOWN")}};
                lifecycle=labels.value(QString::fromStdString(board->lifecycleState),tr("UNKNOWN"));
            }
            const bool dataSource=!node.nativeSenderId.isEmpty();
            bool byteCountValid=false;
            const auto bytes=s.nativeReceivedBytes.toULongLong(&byteCountValid);
            QString diagnostic=s.error;
            for(const auto &detail:{s.nativeReceiverError,s.nativeSourceError,s.nativeReceiverFailureDetail})
                if(!detail.isEmpty())diagnostic+=(diagnostic.isEmpty() ? "" : " | ")+detail;
            if(s.nativeHardwareOutcomeUnknown)diagnostic+=(diagnostic.isEmpty() ? "" : " | ")+tr("Hardware outcome unknown");
            const QStringList values={lifecycle,TransitionName(s.transition),"—",dataSource ? s.process : "—",
                dataSource ? s.nativeFiles : "—",dataSource && byteCountValid ? QString::number(double(bytes)/1e9,'f',3) : "—",
                "—","—","—",s.dataPath,diagnostic};
            for (int col=0; col<values.size(); ++col) {
                auto *item=ui->StatusTable->item(row,col+1);
                item->setText(values[col]);
                item->setToolTip(values[col]);
            }
            if(board) {
                auto *item=ui->StatusTable->item(row,1);
                const auto state=QString::fromStdString(board->lifecycleState);
                projectedLifecycles.insert(s.id,state);
                const bool uncertain=state=="unknown";
                item->setForeground(QColor(uncertain ? "#b53840" : state=="transitioning" ? "#996910" : "#087f5b"));
                item->setTextAlignment(Qt::AlignCenter);
                item->setToolTip(tr("CoBo %1: %2\nLatest operation: %3 · %4 · %5\nThis is the ECC journal projection, not a live hardware readback.")
                    .arg(QString::fromStdString(board->id),lifecycle,QString::fromStdString(board->operation),
                         QString::fromStdString(board->operationPhase),QString::fromStdString(board->operationId)));
            } else paintEccState(row, s);
            paintTransition(row, s);
            if (!node.nativeSenderId.isEmpty() && fConfig.nativeClient.receiverStatusEnabled)
                ui->StatusTable->item(row,4)->setForeground(s.nativeReceiverFault ? QColor("#b53840") : s.nativeReceiverObserved ? QColor("#087f5b") : QColor("#996910"));
            ui->StatusTable->item(row,4)->setToolTip(tr("%1\nAgent: %2\nPID: %3\nSession: %4\nFrames: %5")
                .arg(s.process,node.nativeAgentId,QString::number(s.nativePid),s.nativeSessionId,s.nativeFrames));
            ui->StatusTable->item(row,6)->setToolTip(tr("Received bytes: %1\nThis observation does not establish bytes written to disk.").arg(s.nativeReceivedBytes));
            ui->StatusTable->item(row,11)->setToolTip(tr("%1\nSource: %2\nReceiver stop: %3\nStop operation: %4\nArchive: %5")
                .arg(diagnostic,s.nativeSourceState,s.nativeReceiverFailurePhase,s.nativeReceiverFailureOperation,s.nativeCatalogDir));
            ui->StatusTable->item(row,11)->setForeground(s.nativeSourceFault || s.nativeReceiverFault || s.nativeReceiverFailureHeld || s.nativeHardwareOutcomeUnknown
                ? QColor("#b53840") : QColor("#617383"));
            continue;
        }
        const QStringList values = {s.eccOnline ? StateName(s.state) : "Offline / stale", TransitionName(s.transition),
            mutant ? "—" : (s.sentryCatalogPending ? (s.sentryOnline ? "Archiving" : "Archiving / status unavailable") : s.sentryOnline ? "Online" : "Offline / stale"), mutant ? "—" : s.process,
            mutant ? "—" : QString::number(s.files), mutant ? "—" : QString::number(s.writtenGB, 'f', 3),
            mutant || !s.sentryOnline || !s.sentryRateKnown || s.sentryCatalogPending ? "—" : QString::number(s.rateMB, 'f', 2),
            mutant || s.totalGB <= 0 ? "—" : QString::number(100 * (1 - s.availableGB / s.totalGB), 'f', 1),
            mutant ? "—" : QString::number(s.availableGB, 'f', 1), s.dataPath, s.error + (s.sentryError.isEmpty() ? "" : " | " + s.sentryError)};
        for (int col = 0; col < values.size(); ++col) {
            auto *item=ui->StatusTable->item(row,col+1);
            item->setText(values[col]);
            item->setToolTip(values[col]);
        }
        paintEccState(row, s);
        paintTransition(row, s);
    }
    if (status.isEmpty()) for (int row = 0; row < ui->StatusTable->rowCount(); ++row)
        for (int col = 1; col < ui->StatusTable->columnCount(); ++col) ui->StatusTable->item(row, col)->setText("—");
    if(fConfig.nativeEcc) {
        fNativeReady=!fConfig.nodes.isEmpty();
        for(const auto &node:fConfig.nodes) {
            bool ready=false;
            for(const auto &s:status)if(s.id==node.id)
                ready=s.eccOnline && s.state==4 && s.transition==0 && !s.nativeHardwareOutcomeUnknown;
            const auto projected=projectedLifecycles.value(node.id);
            if(projected=="unknown" || projected=="transitioning")ready=false;
            fNativeReady=fNativeReady && ready;
        }
        ui->StartRun->setEnabled(fConnected && !fBusy && !fActive && !fPlanRetired && fNativeReady);
    }
    UpdateStatusPresentation(status,projectedLifecycles);
    if (!fConfig.nativeEcc) fPlot->AddStatus(status);
    RefreshOfflineToolsGate();
}
