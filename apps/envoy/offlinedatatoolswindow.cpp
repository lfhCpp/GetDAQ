#include "offlinedatatoolswindow.h"
#include "consolepresentation.h"
#include <QCloseEvent>
#include <QAbstractItemModel>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace {
enum Mode { Inspect,Matrix,Merge,Fits,Root,RootViewer };
const char *const programs[]={"getdaq-inspect","getdaq-matrix","getdaq-merge","getdaq-fits","getdaq-root","getdaq-root-viewer"};
}
QString OfflineDataToolsWindow::FindProgram(const QString &name,const QString &directory) {
    QString file=name;
#ifdef Q_OS_WIN
    file+=".exe";
#endif
    const QFileInfo sibling(QDir(directory.isEmpty()?QCoreApplication::applicationDirPath():directory).filePath(file));
    if(sibling.isFile() && sibling.isExecutable())return sibling.absoluteFilePath();
    // An explicit directory is also useful for deterministic packaging tests.
    return directory.isEmpty()?QStandardPaths::findExecutable(file):QString();
}
bool OfflineDataToolsWindow::HasAvailableTool() {
    for(const auto *name:programs)if(!FindProgram(QString::fromLatin1(name)).isEmpty())return true;
    return false;
}
OfflineDataToolsWindow::OfflineDataToolsWindow(QWidget *parent,const QString &programDirectory)
    :QWidget(parent,Qt::Window|Qt::WindowTitleHint|Qt::WindowSystemMenuHint|
             Qt::WindowMinimizeButtonHint|Qt::WindowCloseButtonHint),fProcess(new QProcess(this)) {
    // This is a modeless top-level tool window. Its lifetime must end on every
    // accepted close path, including when it is opened outside MainWindow.
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("OfflineDataToolsWindow");setWindowTitle(tr("Offline data tools"));resize(780,570);
    setStyleSheet(ConsolePresentation::OfflineToolStyleSheet());
    auto *layout=new QVBoxLayout(this);
    fSettings=new QWidget(this);auto *settings=new QVBoxLayout(fSettings);settings->setContentsMargins(0,0,0,0);
    fMode=new QComboBox(fSettings);fMode->setObjectName("OfflineToolMode");
    fMode->addItems({tr("Inspect raw frames"),tr("Convert to waveform matrix (.gmat)"),tr("Merge events (.gmer)"),
                    tr("Export FITS"),tr("Export ROOT"),tr("ROOT event viewer")});
    int first=-1;
    for(int i=0;i<fMode->count();++i) {
        const auto path=FindProgram(QString::fromLatin1(programs[i]),programDirectory);fPrograms.append(path);
        auto *item=qobject_cast<QStandardItemModel *>(fMode->model())->item(i);
        item->setEnabled(!path.isEmpty());
        item->setToolTip(path.isEmpty()?tr("Not installed: %1").arg(programs[i]):path);
        if(!path.isEmpty() && first<0)first=i;
    }
    fMode->setCurrentIndex(first);settings->addWidget(fMode);
    auto *help=new QLabel(tr("Choose completed local data files. Conversion writes to a new output path."));
    help->setWordWrap(true);settings->addWidget(help);
    fInputs=new QListWidget;fInputs->setObjectName("OfflineToolInputs");fInputs->setSelectionMode(QAbstractItemView::ExtendedSelection);settings->addWidget(fInputs);
    auto *inputButtons=new QHBoxLayout;
    auto *add=new QPushButton(tr("Add files…"));fRemove=new QPushButton(tr("Remove selected"));fRemove->setObjectName("OfflineToolRemove");
    inputButtons->addWidget(add);inputButtons->addWidget(fRemove);inputButtons->addStretch();settings->addLayout(inputButtons);
    connect(add,&QPushButton::clicked,this,[this]{
        const auto paths=QFileDialog::getOpenFileNames(this,tr("Select completed raw data"),QString(),tr("Raw data (*.graw);;All files (*)"));
        for(const auto &path:paths)if(fInputs->findItems(path,Qt::MatchExactly).isEmpty())fInputs->addItem(path);
    });
    connect(fRemove,&QPushButton::clicked,this,[this]{qDeleteAll(fInputs->selectedItems());});
    auto *form=new QFormLayout;
    auto *outputRow=new QHBoxLayout;fOutput=new QLineEdit;fOutput->setObjectName("OfflineToolOutput");
    fOutputBrowse=new QPushButton(tr("Browse…"));outputRow->addWidget(fOutput);outputRow->addWidget(fOutputBrowse);
    form->addRow(tr("New output"),outputRow);
    fCount=new QSpinBox;fCount->setObjectName("OfflineToolCount");fCount->setRange(0,1000000000);fCount->setSpecialValueText(tr("All"));
    form->addRow(tr("Frames / merged events"),fCount);settings->addLayout(form);layout->addWidget(fSettings);
    connect(fOutputBrowse,&QPushButton::clicked,this,[this]{
        // FITS requires a NEW directory, so choose its parent/name with the save dialog.
        const auto path=QFileDialog::getSaveFileName(this,tr("Choose a new output path"),fOutput->text());
        if(!path.isEmpty())fOutput->setText(path);
    });
    fStatus=new QLabel;fStatus->setObjectName("OfflineToolStatus");fStatus->setWordWrap(true);layout->addWidget(fStatus);
    fLog=new QPlainTextEdit;fLog->setObjectName("OfflineToolLog");fLog->setReadOnly(true);fLog->setMaximumBlockCount(2000);layout->addWidget(fLog);
    auto *buttons=new QHBoxLayout;fStart=new QPushButton(tr("Run tool"));fStart->setObjectName("OfflineToolStart");
    fStop=new QPushButton(tr("Stop tool"));fStop->setObjectName("OfflineToolStop");fStop->setEnabled(false);
    auto *close=new QPushButton(tr("Close"));close->setObjectName("OfflineToolClose");
    buttons->addWidget(fStart);buttons->addWidget(fStop);buttons->addStretch();buttons->addWidget(close);layout->addLayout(buttons);
    connect(close,&QPushButton::clicked,this,&QWidget::close);
    connect(fMode,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{UpdateMode();});
    connect(fStart,&QPushButton::clicked,this,&OfflineDataToolsWindow::Start);
    connect(fStop,&QPushButton::clicked,this,&OfflineDataToolsWindow::Stop);
    connect(fInputs,&QListWidget::itemSelectionChanged,this,&OfflineDataToolsWindow::UpdateActions);
    connect(fInputs->model(),&QAbstractItemModel::rowsInserted,this,[this]{UpdateActions();});
    connect(fInputs->model(),&QAbstractItemModel::rowsRemoved,this,[this]{UpdateActions();});
    connect(fInputs->model(),&QAbstractItemModel::dataChanged,this,[this]{UpdateActions();});
    connect(fOutput,&QLineEdit::textChanged,this,[this]{UpdateActions();});
    connect(fProcess,&QProcess::stateChanged,this,[this]{UpdateActions();});
    fProcess->setProcessChannelMode(QProcess::MergedChannels);
    connect(fProcess,&QProcess::readyReadStandardOutput,this,&OfflineDataToolsWindow::DrainOutput);
    connect(fProcess,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){Finished(code,status==QProcess::NormalExit);});
    connect(fProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){Append(fProcess->errorString());Finished(-1,false);}
    });
    UpdateMode();
}
OfflineDataToolsWindow::~OfflineDataToolsWindow() {
    // Parent shutdown must not leave an orphan analysis process with an active Run gate.
    fProcess->disconnect(this);
    if(fProcess->state()!=QProcess::NotRunning){fProcess->kill();fProcess->waitForFinished(3000);}
}
void OfflineDataToolsWindow::UpdateMode() {
    const int mode=fMode->currentIndex();const bool output=mode>=Matrix && mode<=Root;
    fInputs->setEnabled(mode!=RootViewer);fOutput->setEnabled(output);fOutputBrowse->setEnabled(output);
    fCount->setEnabled(output);
    fOutput->setPlaceholderText(mode==Fits?tr("New FITS directory (must not exist)"):
        mode==Matrix?tr("New .gmat file"):mode==Merge?tr("New .gmer file"):tr("New .root file"));
    fStatus->setText(mode<0?tr("No offline programs installed. Install them beside GetDAQ or on PATH."):
        mode==RootViewer?tr("Opens the standalone ROOT viewer. Close this tool and its viewer before Run start."):
        mode==Inspect?tr("Checks frame structure and source counts; input files remain unchanged."):
        mode==Merge?tr("Merges by event ID; rotates output at 1024 MiB. Existing output parts are preserved."):
        tr("Input remains unchanged. Output cap: 20 GiB. An existing output path is rejected."));
    UpdateActions();
}
void OfflineDataToolsWindow::UpdateActions() {
    const bool idle=fProcess->state()==QProcess::NotRunning && !fClosing;
    QString error;
    const bool ready=idle && PrepareCommand(nullptr,&error);
    fStart->setEnabled(ready);fStart->setToolTip(ready?tr("Run the selected offline tool."):
        idle?error:tr("Wait for the current tool to finish or close."));
    fRemove->setEnabled(idle && fMode->currentIndex()!=RootViewer && !fInputs->selectedItems().isEmpty());
}
void OfflineDataToolsWindow::Append(const QString &text) {
    fLog->moveCursor(QTextCursor::End);fLog->insertPlainText(text+QLatin1Char('\n'));fLog->ensureCursorVisible();
}
bool OfflineDataToolsWindow::PrepareCommand(QStringList *arguments,QString *error) const {
    const auto fail=[error](const QString &message){if(error)*error=message;return false;};
    const int mode=fMode->currentIndex();
    if(mode<0 || fPrograms.value(mode).isEmpty())return fail(tr("This tool is not installed."));
    QStringList inputs,args;
    for(int i=0;mode!=RootViewer && i<fInputs->count();++i) {
        const QFileInfo file(fInputs->item(i)->text());
        if(!file.isFile() || !file.isReadable())return fail(tr("Input is not a readable file: %1").arg(file.filePath()));
        inputs.append(file.absoluteFilePath());
    }
    if(mode!=RootViewer && inputs.isEmpty())return fail(tr("Add at least one input file."));
    if(mode!=Inspect && mode!=Merge && mode!=RootViewer && inputs.size()!=1)return fail(tr("This converter accepts exactly one input file."));
    if(mode==Inspect)args=inputs;
    else if(mode!=RootViewer) {
        const QString output=QFileInfo(fOutput->text().trimmed()).absoluteFilePath();
        if(fOutput->text().trimmed().isEmpty() || QFileInfo::exists(output) || QFileInfo(output).isSymLink()) {
            return fail(tr("Choose a new output path; an existing path cannot be replaced."));
        }
        if(!QFileInfo(QFileInfo(output).absolutePath()).isDir())return fail(tr("The output parent directory does not exist."));
        if(mode==Merge) {args<<"--output"<<output<<"--max-events"<<QString::number(fCount->value());args+=inputs;}
        else {args<<"--input"<<inputs.first()<<(mode==Fits?"--output-dir":"--output")<<output
                  <<"--count"<<QString::number(fCount->value())<<"--max-output-bytes"<<"21474836480";}
    }
    if(arguments)*arguments=args;
    if(error)error->clear();
    return true;
}
void OfflineDataToolsWindow::Start() {
    if(fProcess->state()!=QProcess::NotRunning || fClosing)return;
    QStringList args;QString error;
    if(!PrepareCommand(&args,&error)){fStatus->setText(error);UpdateActions();return;}
    const int mode=fMode->currentIndex();
    fLog->clear();fDisplayedBytes=0;fCancelled=false;fSettings->setEnabled(false);fStart->setEnabled(false);fStop->setEnabled(true);
    fStatus->setText(tr("Running %1…").arg(programs[mode]));
    // Separate argv entries preserve spaces/metacharacters; never invoke a shell.
    fProcess->start(fPrograms[mode],args);
}
void OfflineDataToolsWindow::DrainOutput() {
    const QByteArray bytes=fProcess->readAllStandardOutput();const qint64 limit=256*1024;
    if(fDisplayedBytes<limit) {
        fLog->moveCursor(QTextCursor::End);fLog->insertPlainText(QString::fromLocal8Bit(bytes.left(int(limit-fDisplayedBytes))));
        fDisplayedBytes+=bytes.size();
        if(fDisplayedBytes>=limit)Append(tr("\nDisplay limit reached (256 KiB); remaining process output is discarded."));
    }
}
void OfflineDataToolsWindow::Finished(int exitCode,bool normal) {
    DrainOutput();fSettings->setEnabled(true);fStop->setEnabled(false);UpdateActions();
    fStatus->setText(fCancelled?tr("Tool stopped. Check the output path before reusing any incomplete export."):
        normal && exitCode==0?tr("Completed successfully."):tr("Tool failed (exit %1). See the original output below.").arg(exitCode));
    // A close request during execution already hid the top-level window and
    // disabled WA_DeleteOnClose so its QProcess could finish asynchronously.
    // Delete explicitly here: the user already closed the visible window, so
    // process completion should finish its lifetime without relying on another
    // close event from the platform window system.
    if(fClosing)deleteLater();
}
void OfflineDataToolsWindow::Stop() {
    if(fProcess->state()==QProcess::NotRunning)return;
    fCancelled=true;fStop->setEnabled(false);fStatus->setText(tr("Stopping tool…"));fProcess->terminate();
    QTimer::singleShot(2000,fProcess,[this]{if(fProcess->state()!=QProcess::NotRunning && fCancelled)fProcess->kill();});
}
void OfflineDataToolsWindow::closeEvent(QCloseEvent *event) {
    if(fProcess->state()!=QProcess::NotRunning){
        // Close the visible window immediately, but keep the object alive until
        // its child has exited. Finished() then schedules deletion and releases
        // the Run gate. Temporarily disable delete-on-close so
        // stopping a converter never blocks the GUI in the destructor.
        if(!fClosing){fClosing=true;setAttribute(Qt::WA_DeleteOnClose,false);emit ClosePendingSignal();}
        Stop();
        event->accept();
        return;
    }
    event->accept();
    // Do not rely on a window-manager close path honoring WA_DeleteOnClose.
    // This window is also parented to MainWindow, so explicit deferred
    // destruction makes the QPointer/menu gate release deterministic for the
    // title-bar X, the in-window Close button, and Escape alike.
    if(!fClosing){fClosing=true;setAttribute(Qt::WA_DeleteOnClose,false);deleteLater();}
}
void OfflineDataToolsWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key()==Qt::Key_Escape){close();event->accept();return;}
    QWidget::keyPressEvent(event);
}
