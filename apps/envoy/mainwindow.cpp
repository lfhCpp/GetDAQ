#include "mainwindow.h"
#include "offlinedatatoolswindow.h"
#include "ui_mainwindow.h"
#include "runcontrol.h"
#include "ipwindow.h"
#include "widgetplot2d.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include "diagnostichighlighter.h"
#include <QTextBlock>
#include <QComboBox>
#include <QSignalBlocker>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QMenuBar>
#include <QMenu>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QScrollBar>
#include <QSaveFile>
#include <QTimer>
#include <QToolButton>
#include <QApplication>
#include <stdexcept>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), fConfig(RunConfig::Default()), fRunControl(new RunControl)
{
    ui->setupUi(this);
    InitializeConsole();
    InitializePar();
    ArrangeConsole();
}
void MainWindow::InitializePar()
{
    InitializeMenus();
    qRegisterMetaType<RunConfig>(); qRegisterMetaType<NodeStatus>(); qRegisterMetaType<QVector<NodeStatus> >();
    fPlot = new WidgetPlot2D(this); fPlot->setMinimumHeight(132); ui->PlotLayout->addWidget(fPlot);
    ui->StatusTable->setColumnCount(12);
    ui->StatusTable->setHorizontalHeaderLabels({"Node", "ECC", "Transition", "Sentry", "Process", "Files", "Written GB", "MB/s", "Disk used %", "Free GB", "Data path", "Error", ""});
    ui->StatusTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->StatusTable->horizontalHeader()->setStretchLastSection(true);
    ui->StatusTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->StatusTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->StatusTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->Fields->setColumnCount(2); ui->Fields->setHorizontalHeaderLabels({"Parameter", "Value"});
    ui->Fields->verticalHeader()->setDefaultSectionSize(ConsolePresentation::ParameterRowHeight);
    ui->Fields->verticalHeader()->setMinimumSectionSize(ConsolePresentation::ParameterRowHeight);
    ui->Fields->verticalHeader()->setVisible(false);
    ui->Fields->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->Fields->horizontalHeader()->setFixedHeight(24);
    ui->Fields->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    ui->Fields->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->Fields->setColumnWidth(0, 150);
    ui->Log->document()->setMaximumBlockCount(2000);
    const bool applicationLogOpened=fLogStore.OpenApplicationLog();
    InitializeRunControl();
    SetConfig(fConfig);
    ShowControlSlot("Disconnected", false, false, false);
    ShowLogSlot("GetDAQ ready. Configure the ECC connection and data receiver before connecting.");
    if (!applicationLogOpened) ShowLogSlot("Cannot open application log in user data, cache or temporary directories: " + fLogStore.ErrorString());
    else if (fLogStore.UsedFallbackDirectory())
        ShowLogSlot("Application data directory is not writable; logging to fallback: " + fLogStore.ApplicationLogPath());
    QTimer *clock = new QTimer(this);
    connect(clock, &QTimer::timeout, this, [this]() {
        if (fDisplayClock.isValid()) ui->Elapsed->setText(ConsolePresentation::ElapsedText(fDisplayClock.elapsed()));
    });
    clock->start(1000);
}
MainWindow::~MainWindow()
{
    if(fWaveformViewer){fWaveformViewer->disconnect(this);delete fWaveformViewer.data();}
    if(fDataTools){fDataTools->disconnect(this);delete fDataTools.data();}
    if (fControlThread.isRunning()) { emit ShutdownSignal(); fControlThread.wait(); }
    if (fLogStore.HasConnectionLog()) EndConnectionLog(tr("Application closed"));
    delete ui;
}

void MainWindow::ResizeStatusTableToContents()
{
    auto *table=ui->StatusTable;
    int rowsHeight=0;
    for(int row=0;row<table->rowCount();++row)rowsHeight+=table->rowHeight(row);
    const int horizontalScrollHeight=table->horizontalHeader()->length()>table->viewport()->width()
        ? table->horizontalScrollBar()->sizeHint().height() : 0;
    const int contentHeight=table->horizontalHeader()->height()+rowsHeight
        +2*table->frameWidth()+horizontalScrollHeight;
    const auto margins=ui->StatusLayout->contentsMargins();
    // Use the actual control bar, including wrapped order text, rather than a
    // fixed allowance that leaves blank space after reducing the node count.
    const int barWidth=qMax(1,ui->StatusPanel->width()-margins.left()-margins.right());
    const int barHeight=qMax(ui->NodeControls->sizeHint().height(),ui->NodeControls->heightForWidth(barWidth));
    const int overhead=margins.top()+margins.bottom()+ui->StatusLayout->spacing()+barHeight;
    const int available=qMax(table->horizontalHeader()->height()+ConsolePresentation::RowHeight,
        this->height()-ui->MainContentSplitter->maximumHeight()-ui->HeaderPanel->sizeHint().height()
        -qMax(ui->PlotPanel->isHidden() ? 0 : ui->PlotPanel->minimumHeight(),ui->LogPanel->minimumHeight())
        -ui->MainLayout->spacing()*2-ui->VerticalSplitter->handleWidth()-12-overhead);
    const int tableHeight=qMin(contentHeight,available);
    // Cap empty rows while retaining the vertical splitter: users can give
    // more room to the plot/log and scroll the node table when needed.
    const int minimumTableHeight=qMin(contentHeight,table->horizontalHeader()->height()
        +qMin(3,table->rowCount())*ConsolePresentation::RowHeight+2*table->frameWidth()+horizontalScrollHeight);
    table->setMinimumHeight(minimumTableHeight);table->setMaximumHeight(contentHeight);
    ui->StatusPanel->setMinimumHeight(minimumTableHeight+overhead);
    ui->StatusPanel->setMaximumHeight(contentHeight+overhead);
    ui->StatusLayout->activate();
    const int panelHeight=tableHeight+overhead;
    if(ui->VerticalSplitter->sizes().value(0)!=panelHeight)
        ui->VerticalSplitter->setSizes({panelHeight,qMax(1,ui->VerticalSplitter->height()-panelHeight-ui->VerticalSplitter->handleWidth())});
}


void MainWindow::ExportVisibleLog()
{
    const QString path=QFileDialog::getSaveFileName(this,tr("Export visible operation log"),
        QDir::home().filePath(QString("getdaq-log-%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss"))),
        tr("Text files (*.txt);;Log files (*.log);;All files (*)"));
    if(path.isEmpty())return;
    QSaveFile output(path);
    if(!output.open(QIODevice::WriteOnly|QIODevice::Text)) {
        ShowLogSlot(tr("Log export failed: %1").arg(output.errorString()));return;
    }
    const QByteArray content=(ui->Log->toPlainText()+QLatin1Char('\n')).toUtf8();
    if(output.write(content)!=content.size()||!output.commit()) {
        output.cancelWriting();ShowLogSlot(tr("Log export failed: %1").arg(output.errorString()));return;
    }
    ShowLogSlot(tr("Visible log exported to %1").arg(QFileInfo(path).absoluteFilePath()));
}




void MainWindow::closeEvent(QCloseEvent *event)
{
    if (fCanClose) { event->accept(); return; }
    event->ignore();
    if (fClosing) return;
    if (fActive || fBusy || fRemoteRunning) {
        if (QMessageBox::question(this, tr("Acquisition may still be active"),
            tr("Closing does not stop remote acquisition. An interrupted journal will be kept. Close the controller anyway?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    }
    fClosing = true;
    if(fWaveformViewer)fWaveformViewer->close();
    if(fDataTools)fDataTools->close();
    setEnabled(false); emit ShutdownSignal();
}
