#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include "diagnostichighlighter.h"
#include <QDialog>
#include <QComboBox>
#include <QHeaderView>
#include <QFontDatabase>
#include <QMenuBar>
#include <QStatusBar>
#include <QStyle>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>

using namespace ConsolePresentation;
void MainWindow::InitializeConsole()
{
    setMinimumSize(1040,720);resize(1536,1024);
    setStyleSheet(ConsolePresentation::StyleSheet());menuBar()->hide();statusBar()->hide();
    ui->MainLayout->setContentsMargins(8,2,8,8);ui->MainLayout->setSpacing(8);
    ui->TitleLayout->setContentsMargins(8,0,8,0);ui->TitleLayout->setSpacing(12);
    ui->HeaderMenuLayout->setContentsMargins(8,0,0,0);ui->HeaderMenuLayout->setSpacing(4);
    ui->HeaderMenuPanel->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);
    ui->ConnectionStatusLayout->setContentsMargins(10,6,10,6);
    ui->LeftContentLayout->setContentsMargins(0,0,0,0);ui->LeftContentLayout->setSpacing(8);
    for(auto *layout:{ui->SetupPanelLayout,ui->RunPanelLayout}) {
        layout->setContentsMargins(12,10,12,10);layout->setSpacing(8);
        layout->setAlignment(Qt::AlignTop);
    }
    ui->SetupPanelLayout->setContentsMargins(12,14,12,14);ui->SetupPanelLayout->setSpacing(16);
    ui->RunPanelLayout->setContentsMargins(12,12,12,12);ui->RunPanelLayout->setSpacing(12);
    ui->SetupPanelTitle->setFixedHeight(24);ui->RunPanelTitle->setFixedHeight(24);
    ui->SetupInputsLayout->setContentsMargins(0,0,0,0);ui->SetupInputsLayout->setSpacing(12);ui->SetupInputsLayout->setAlignment(Qt::AlignVCenter);
    ui->SetupContentLayout->setContentsMargins(0,0,0,0);ui->SetupContentLayout->setSpacing(12);
    // Let the automatic controls use the full setup height, including the title row.
    ui->SetupPanelLayout->takeAt(0);ui->SetupTitleLayout->setParent(nullptr);
    ui->SetupInputsLayout->insertLayout(0,ui->SetupTitleLayout);
    ui->SetupContentLayout->setAlignment(Qt::AlignVCenter);
    fAutoRunWidget=new AutoRunWidget(ui->SetupPanel);ui->SetupContentLayout->addWidget(fAutoRunWidget);
    ui->SetupContentLayout->setStretch(0,1);ui->SetupContentLayout->setStretch(1,0);
    connect(fAutoRunWidget,&AutoRunWidget::ApplyRequested,this,&MainWindow::ApplyAutoRunSettings);
    connect(fAutoRunWidget,&AutoRunWidget::EnableRequested,this,[this](bool enabled){emit SetAutoRunEnabledSignal(enabled);});
    connect(ui->AutoRunSettingsButton,&QToolButton::clicked,this,[this]{
        auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("AutoRunDialog");dialog->setWindowTitle(tr("Automatic Run"));
        dialog->setStyleSheet(ConsolePresentation::StyleSheet().replace("#centralwidget","#AutoRunDialog"));
        auto *layout=new QVBoxLayout(dialog);auto *controls=new AutoRunWidget(dialog);controls->SetSettings(fAutoRunWidget->Settings());controls->SetLocked(fActive || fAutomaticArmed);layout->addWidget(controls);
        connect(controls,&AutoRunWidget::EnableRequested,this,[this](bool enabled){emit SetAutoRunEnabledSignal(enabled);});
        connect(controls,&AutoRunWidget::ApplyRequested,this,[this](const AutoRunSettings &settings){ApplyAutoRunSettings(settings);});
        dialog->show();
    });
    fLogHighlighter=new DiagnosticHighlighter(ui->Log->document());
    ui->ConfigLayout->setSpacing(10);ui->ConfigLayout->setAlignment(Qt::AlignLeft);ui->RunLayout->setSpacing(8);
    ui->Controls->setHorizontalSpacing(6);ui->Controls->setVerticalSpacing(8);
    ui->ConfigPath->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    ui->Experiment->setMinimumWidth(85);ui->RunNumber->setFixedWidth(120);
    for(auto *input:QList<QWidget *>{ui->Experiment,ui->RunNumber,ui->Description})input->setFixedHeight(36);
    ui->Description->setMinimumWidth(100);
    for(auto *b:{ui->LoadConfig,ui->SaveConfig,ui->Nodes}) {
        b->setMinimumSize(120,36);b->setMaximumWidth(172);b->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    }
    ui->LoadConfig->setIcon(Icon(Folder));ui->SaveConfig->setIcon(Icon(Save));ui->Nodes->setIcon(Icon(Folder));
    ui->StartRun->setIcon(Icon(Play,Qt::white));ui->StopRun->setIcon(Icon(Stop,QColor("#b53840")));
    ui->Connect->setIcon(Icon(Link));ui->Disconnect->setIcon(Icon(Unlink,QColor("#b53840")));
    ui->Regress->setIcon(Icon(Back,QColor("#b53840")));ui->Progress->setIcon(Icon(Forward,QColor("#087f5b")));
    ui->Finalize->setIcon(Icon(Retry));ui->Recover->setIcon(Icon(Document));
    ui->NodeRegress->setIcon(Icon(Back));ui->NodeProgress->setIcon(Icon(Forward));
    ui->AddField->setIcon(Icon(Plus));ui->RemoveField->setIcon(Icon(Trash));
    for(auto *b:findChildren<QPushButton *>())b->setIconSize(QSize(18,18));
    ui->FieldsColumnLayout->setContentsMargins(0,0,0,0);
    ui->FieldsLayout->setContentsMargins(10,8,10,8);ui->FieldsLayout->setSpacing(6);
    ui->FieldsColumn->setMinimumWidth(298);ui->FieldsColumn->setMaximumWidth(380);
    ui->MainContentSplitter->setHandleWidth(8);ui->MainContentSplitter->setChildrenCollapsible(false);
    ui->MainContentSplitter->setSizes({1200,316});
    ui->MainContentSplitter->setStretchFactor(0,1);ui->MainContentSplitter->setStretchFactor(1,0);
    connect(ui->MainContentSplitter,&QSplitter::splitterMoved,this,[this]{ArrangeConsole();});

    auto *header=new QWidget(ui->LeftColumn);header->setObjectName("StatusOverview");
    auto *headerLayout=new QHBoxLayout(header);headerLayout->setContentsMargins(0,0,0,0);headerLayout->setSpacing(8);
    auto *title=new QLabel(tr("System and receiver status"),header);title->setObjectName("StatusSectionTitle");
    title->setToolTip(tr("Select a row to inspect node details."));
    headerLayout->addWidget(title);headerLayout->addStretch();
    for(const auto &name:{"CoBoOverview","RateOverview","ErrorOverview"}) {
        auto *label=new QLabel(header);label->setObjectName(name);label->setProperty("summary",true);headerLayout->addWidget(label);
    }
    ui->LeftContentLayout->addWidget(header);
    // Keep acquisition controls at their content height; only setup absorbs the
    // small difference needed to display the full parameter table alongside it.
    ui->LeftContentLayout->setStretch(0,1);ui->LeftContentLayout->setStretch(1,0);
    ui->SetupPanelLayout->setAlignment(Qt::Alignment());
    ui->RunPanel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Fixed);
    ui->StatusLayout->setContentsMargins(10,7,10,7);ui->StatusLayout->setSpacing(6);
    auto *table=ui->StatusTable;
    table->verticalHeader()->setMinimumSectionSize(RowHeight);table->verticalHeader()->setDefaultSectionSize(RowHeight);
    table->verticalHeader()->hide();table->setAlternatingRowColors(true);table->setShowGrid(true);
    table->setWordWrap(false);table->setMouseTracking(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    table->horizontalHeader()->setMinimumSectionSize(LampWidth);table->horizontalHeader()->setFixedHeight(26);
    auto *selector=new QComboBox(ui->StatusPanel);selector->setObjectName("NodeSelector");selector->setMaximumWidth(150);
    selector->setToolTip(tr("Select a configured node; this selects the same row used by the node controls."));
    ui->NodeControls->insertWidget(1,selector);
    selector->setMinimumWidth(120);selector->setFixedHeight(34);
    for(auto *b:{ui->NodeRegress,ui->NodeProgress})b->setFixedHeight(34);
    ui->NodeControls->setSpacing(8);ui->OrderNote->setWordWrap(true);
    ui->OrderNote->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    ui->NodeControls->setStretch(ui->NodeControls->indexOf(ui->OrderNote),1);
    ui->OrderNote->setAlignment(Qt::AlignRight|Qt::AlignVCenter);
    connect(selector,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int row) {
        if(row>=0 && row<ui->StatusTable->rowCount())ui->StatusTable->selectRow(row);
    });
    connect(table,&QTableWidget::itemSelectionChanged,this,[this,selector] {
        const QSignalBlocker block(selector);selector->setCurrentIndex(ui->StatusTable->currentRow());
        ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
    });
    for(auto *layout:{ui->PlotLayout,ui->LogPanelLayout}) {layout->setContentsMargins(10,7,10,7);layout->setSpacing(4);}
    ui->PlotPanel->setMinimumHeight(150);ui->LogPanel->setMinimumHeight(76);
    ui->VerticalSplitter->setChildrenCollapsible(false);ui->VerticalSplitter->setHandleWidth(8);
    ui->VerticalSplitter->setStretchFactor(0,0);ui->VerticalSplitter->setStretchFactor(1,1);
    ui->BottomPanels->setChildrenCollapsible(false);ui->BottomPanels->setHandleWidth(8);
    ui->BottomPanels->setStretchFactor(0,1);ui->BottomPanels->setStretchFactor(1,1);
    ui->BottomPanels->setSizes({1000,1000});
    ui->Log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    ui->VerticalSplitter->setSizes({340,330});
    ui->MainLayout->setStretch(0,0);ui->MainLayout->setStretch(1,0);ui->MainLayout->setStretch(2,1);
    QTimer::singleShot(0,this,[this]{ArrangeConsole();});
}
void MainWindow::UpdateStatusPresentation(const QVector<NodeStatus> &status,const QMap<int,QString> &lifecycles)
{
    auto overview=Summarize(fConfig,status);
    QMap<int,NodeStatus> byId;for(const auto &s:status)byId.insert(s.id,s);
    for(const auto &node:fConfig.nodes) {
        const auto state=lifecycles.value(node.id);const auto found=byId.constFind(node.id);
        if(node.mutant || found==byId.constEnd() || (state!="unknown" && state!="transitioning"))continue;
        const auto &s=found.value();
        if(s.eccOnline && s.state==4 && s.transition==0)--overview.ready;
        if(state=="unknown" && NodeHealth(&s,false,fConfig)!=Fault)++overview.errors;
    }
    const auto set=[this](const char *name,const QString &text,const QString &tip,bool fault) {
        auto *label=findChild<QLabel *>(name);if(!label)return;
        label->setText(text);label->setToolTip(tip);label->setProperty("health",fault ? "fault" : "normal");
        label->style()->unpolish(label);label->style()->polish(label);
    };
    set("CoBoOverview",tr("%1 CoBos ready").arg(overview.ready),
        tr("Configured CoBos with a fresh ECC Ready state and stable transition. MuTaNT is excluded. Unknown nodes are not ready."),false);
    set("RateOverview",overview.rateKnown ? tr("%1 MB/s").arg(overview.rateMB,0,'f',2) : tr("Rate: —"),
        tr("Sum of current CoBo Sentry rates in MB/s. Unknown if any configured CoBo is unobserved."),false);
    set("ErrorOverview",overview.errors ? tr("%1 node(s) with errors").arg(overview.errors) : overview.errorsKnown ? tr("No errors") : tr("Errors: unknown"),
        tr("Nodes with reported control, receiver, source or Sentry faults. Missing observations are unknown, not healthy."),overview.errors>0);
    const int column=ui->StatusTable->columnCount()-1;
    for(int row=0;row<fConfig.nodes.size();++row) {
        const auto &node=fConfig.nodes[row];const auto found=byId.constFind(node.id);
        const auto *s=found==byId.constEnd() ? nullptr : &found.value();
        auto *lamp=ui->StatusTable->item(row,column);if(!lamp)continue;
        auto health=NodeHealth(s,node.mutant,fConfig);
        if(lifecycles.value(node.id)=="unknown")health=Fault;
        else if(lifecycles.value(node.id)=="transitioning" && health!=Fault)health=Transitioning;
        lamp->setData(LampRole,int(health));
        const auto tooltip=s ? tr("%1\nECC: %2 · %3\nReceiver/Sentry: %4\n%5%6%7%8")
            .arg(node.name,s->eccOnline ? StateName(s->state) : tr("Offline / stale"),TransitionName(s->transition),
                 node.mutant ? tr("Not applicable (MuTaNT)") : s->sentryOnline ? tr("Online") : tr("Offline / stale"),
                 s->error,s->sentryError,QString(),QString()) : tr("%1: not observed").arg(node.name);
        const auto detail=tooltip+(lifecycles.contains(node.id) ? tr("\nCoBo lifecycle: %1 (journal projection)").arg(lifecycles.value(node.id)) : QString());
        lamp->setToolTip(detail);lamp->setData(Qt::AccessibleTextRole,detail);
    }
}
