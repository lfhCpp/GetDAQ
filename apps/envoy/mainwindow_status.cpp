#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgetplot2d.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include <QAction>
#include <QColor>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <stdexcept>

void MainWindow::ShowStatusSlot(QVector<NodeStatus> status)
{
    fLastNodeStatus=status;
    fObservedEccState=-1;
    if(!status.isEmpty()){int state=status[0].state;bool consistent=true;for(const auto &node:status)consistent=consistent && node.eccOnline && node.transition==0 && node.state==state;if(consistent)fObservedEccState=state;}
    QMap<int,QString> projectedLifecycles;
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
            : tr("Transition of this selected ECC node."));
    };
    fRemoteRunning = false;
    for (const NodeStatus &s : status) if (s.state == 5 || s.state == 6) fRemoteRunning = true;
    for (const NodeStatus &s : status) for (int row = 0; row < ui->StatusTable->rowCount(); ++row) {
        if (ui->StatusTable->item(row, 0)->data(Qt::UserRole).toInt() != s.id) continue;
        const bool mutant = fConfig.nodes[row].mutant;
        
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
    
    UpdateStatusPresentation(status,projectedLifecycles);
    fPlot->AddStatus(status);
    RefreshOfflineToolsGate();
}
