#include "servicelogpanel.h"
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "autorunwidget.h"
#include "consolepresentation.h"
#include "widgetplot2d.h"
#include <QAction>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QScrollBar>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>

void MainWindow::SetConfig(const RunConfig &config)
{
    fAutoRunWidget->SetSettings(config.autoRun);
    ui->ResumeCatalogReplacement->hide();ui->CatalogRecoveryReason->hide();
    if(!fLogStore.SetRecordsDirectory(config.outputDir))ShowLogSlot(tr("GUI log path warning: %1").arg(fLogStore.ErrorString()));
    fLastNodeStatus.clear();fConfig=config;if(!fOnlineSourcesPath.isEmpty())OnlineSourcesFile();if(fServiceLogs)fServiceLogs->SetProject(config.deploymentProject);statusBar()->hide();
    fPlot->setVisible(true);
    ui->PlotPanel->setVisible(true);
    ui->BottomPanels->setSizes({1000,1000});
    ui->StatusTable->setColumnCount(13);
    ui->StatusTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    for (auto *action:{fPause,fResume}) action->setVisible(true);
    ui->OrderNote->setText(tr("Prepare: MuTaNT first  ·  Configure / Start: CoBos first  ·  Stop: MuTaNT first"));
    ui->NodeProgress->setText(tr("Progress selected node"));
    ui->NodeRegress->setText(tr("Regress selected node"));
    ui->NodeRegress->setVisible(true);
    for(auto *button:{ui->NodeProgress,ui->NodeRegress}) {
        button->setProperty("scope","legacy-node");
        button->setToolTip(QString());
    }
    ui->StatusTable->setHorizontalHeaderLabels({"Node", "ECC", "Transition", "Sentry", "Process", "Files", "Data GB", "MB/s", "Disk %", "Free GB", "Data path", "Error", ""});
    const QList<int> widths{100,78,90,82,140,58,78,78,78,80,210,150};
    const QStringList tips={tr("Configured node."),
        tr("Node lifecycle state."),
        tr("Transition number."),
        tr("Sentry file and receiver process observation."),tr("Receiver process observation."),
        tr("Receiver file count."),tr("Written data in decimal gigabytes."),
        tr("Recent write rate in megabytes per second; unavailable without a rate observation."),tr("Data-volume usage percent."),
        tr("Available disk space in gigabytes."),tr("Data directory."),tr("Latest diagnostic; hover for receiver and source details.")};
    for(int column=0;column<widths.size();++column) {
        ui->StatusTable->setColumnWidth(column,widths[column]);
        ui->StatusTable->horizontalHeaderItem(column)->setToolTip(tips[column]);
    }
    ui->Experiment->setText(config.experiment); {const QSignalBlocker blocked(ui->RunNumber);ui->RunNumber->setValue(config.runNumber);}
    ui->Description->setText(config.description);
    ui->ConfigPath->clear();
    ui->ConfigPath->hide();
    ui->Fields->setRowCount(config.fields.size());
    int row = 0;
    for (auto it = config.fields.constBegin(); it != config.fields.constEnd(); ++it, ++row) {
        ui->Fields->setRowHeight(row,ConsolePresentation::ParameterRowHeight);
        ui->Fields->setItem(row, 0, new QTableWidgetItem(it.key())); ui->Fields->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
    ui->StatusTable->setRowCount(config.nodes.size());
    for (int i = 0; i < config.nodes.size(); ++i) {
        ui->StatusTable->setRowHeight(i,ConsolePresentation::RowHeight);
        for (int col = 0; col < ui->StatusTable->columnCount(); ++col) ui->StatusTable->setItem(i, col, new QTableWidgetItem("—"));
        for(int col=5;col<=9;++col)
            ui->StatusTable->item(i,col)->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
        ui->StatusTable->item(i,3)->setTextAlignment(Qt::AlignCenter);
        ui->StatusTable->item(i, 0)->setText(config.nodes[i].name);
        ui->StatusTable->item(i, 0)->setData(Qt::UserRole, config.nodes[i].id);
    }
    const int lampColumn=ui->StatusTable->columnCount()-1;
    // Logical indices of existing data/control columns stay unchanged. Only the
    // visual header order places the new independent health column first.
    for(int column=0;column<lampColumn;++column)ui->StatusTable->setItemDelegateForColumn(column,nullptr);
    ui->StatusTable->setItemDelegateForColumn(lampColumn,new ConsolePresentation::LampDelegate(ui->StatusTable));
    for(int column=0;column<ui->StatusTable->columnCount();++column)
        ui->StatusTable->horizontalHeader()->moveSection(ui->StatusTable->horizontalHeader()->visualIndex(column),column);
    ui->StatusTable->horizontalHeader()->moveSection(ui->StatusTable->horizontalHeader()->visualIndex(lampColumn),0);
    ui->StatusTable->horizontalHeader()->setSectionResizeMode(lampColumn,QHeaderView::Fixed);
    ui->StatusTable->setColumnWidth(lampColumn,ConsolePresentation::LampWidth);
    if(auto *selector=findChild<QComboBox *>("NodeSelector")) {
        const QSignalBlocker blocker(selector);selector->clear();
        for(const auto &node:config.nodes)selector->addItem(node.name,node.id);
        if(ui->StatusTable->currentRow()<0 && !config.nodes.isEmpty())ui->StatusTable->selectRow(0);
        selector->setCurrentIndex(ui->StatusTable->currentRow());
        selector->setVisible(!config.nodes.isEmpty());
    }
    ui->ConfigPath->setToolTip(QString());
    UpdateStatusPresentation({});
    ResizeStatusTableToContents();
    fPlot->SetNodes(config.nodes);
    fPlot->setToolTip(QString());
    ArrangeConsole();
    QTimer::singleShot(0,this,[this]{ArrangeConsole();ui->StatusTable->horizontalScrollBar()->setValue(0);});
    ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
}
