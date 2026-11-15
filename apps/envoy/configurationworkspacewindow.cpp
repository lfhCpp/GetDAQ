#include "configurationworkspacewindow.h"
#include "ui_configurationworkspacewindow.h"
#include "workspacestructurewindow.h"
#include "workspacerecoverywindow.h"
#include "workspacemigrationwindow.h"
#include "xcfgsavejournal.h"
#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTabWidget>

namespace {
QString EditorText(const QByteArray &bytes) {
    QString text=QString::fromUtf8(bytes);text.replace("\r\n","\n");text.replace('\r','\n');return text;
}
}
ConfigurationWorkspaceWindow::ConfigurationWorkspaceWindow(const QString &directory,bool readOnly,QWidget *parent)
    : QDialog(parent),ui(new Ui::ConfigurationWorkspaceWindow),fDirectory(directory),fReadOnly(readOnly)
{
    ui->setupUi(this);
    ui->reviewAll->setEnabled(!fReadOnly);
    if(fReadOnly) ui->status->setText(tr("Connected/active mode: source viewing only. Editing and saving are disabled."));
    connect(ui->openRoot,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::ChooseRoot);
    connect(ui->reviewAll,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::ReviewAll);
    connect(ui->saveAll,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::SaveAll);
    connect(ui->undo,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::Undo);
    connect(ui->redo,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::Redo);
    connect(ui->structure,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::EditStructure);
    connect(ui->recovery,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::InspectRecovery);
    connect(ui->migration,&QPushButton::clicked,this,&ConfigurationWorkspaceWindow::MigrateSource);
    RefreshHistory();
}
ConfigurationWorkspaceWindow::~ConfigurationWorkspaceWindow() { delete ui; }

bool ConfigurationWorkspaceWindow::Open(const QString &path,QString *error) {
    // An unreviewed editor buffer must not be silently replaced by opening another root.
    if(HasPendingText()) { *error=tr("Review pending source text before adding another root.");return false; }
    if(!fWorkspace.Open(path,error)) {
        QString pending,message;const QFileInfo file(path);auto source=file.canonicalFilePath();
        if(source.isEmpty()) {const auto parent=QFileInfo(file.absolutePath()).canonicalFilePath();if(!parent.isEmpty())source=QDir(parent).filePath(file.fileName());}
        if(!source.isEmpty() && XcfgSaveJournal::Pending(source,&pending,&message) && !pending.isEmpty())fRecoveryDirectory=pending;
        return false;
    }
    AddSourceTabs();InvalidateReview();
    ui->roots->setText(tr("Opened roots:\n%1").arg(fWorkspace.Roots().join("\n")));
    return true;
}
void ConfigurationWorkspaceWindow::AddSourceTabs() {
    for(const auto &file:fWorkspace.Files()) {
        if(fEditors.contains(file)) continue;
        auto editor=new QPlainTextEdit(ui->sourceTabs);editor->setObjectName("WorkspaceSourceEditor");
        editor->setProperty("sourceFile",file);editor->setPlainText(QString::fromUtf8(fWorkspace.Bytes(file)));
        editor->setReadOnly(fReadOnly || fWorkspace.RequiresReopen());
        const auto index=ui->sourceTabs->addTab(editor,QFileInfo(file).fileName());
        ui->sourceTabs->setTabToolTip(index,file);fEditors[file]=editor;
        connect(editor,&QPlainTextEdit::textChanged,this,&ConfigurationWorkspaceWindow::InvalidateReview);
    }
}
bool ConfigurationWorkspaceWindow::HasPendingText() const {
    return !fPendingFiles.isEmpty();
}
void ConfigurationWorkspaceWindow::ChooseRoot() {
    const auto path=QFileDialog::getOpenFileName(this,tr("Add a root configuration"),fDirectory,tr("GET XML (*.xcfg *.xml)"));
    if(path.isEmpty()) return;
    QString error;if(!Open(path,&error)) ui->status->setText(error);
}
void ConfigurationWorkspaceWindow::InvalidateReview() {
    // A keystroke checks only its source, not all 256 possible source tabs.
    if(auto editor=qobject_cast<QPlainTextEdit *>(sender())) {
        const auto file=editor->property("sourceFile").toString();
        if(editor->toPlainText()==EditorText(fWorkspace.Bytes(file)))fPendingFiles.remove(file);else fPendingFiles.insert(file);
    }
    fHaveReview=false;ui->saveAll->setEnabled(false);ui->review->clear();
    RefreshHistory();
    if(!fReadOnly) ui->status->setText(tr("Changes are in memory. Review all sources before saving; no hardware command is sent."));
}
void ConfigurationWorkspaceWindow::RefreshHistory() {
    const bool allowed=!fReadOnly && !fWorkspace.RequiresReopen() && !HasPendingText();
    ui->undo->setEnabled(allowed && fWorkspace.CanUndo());ui->redo->setEnabled(allowed && fWorkspace.CanRedo());
    ui->structure->setEnabled(allowed && !fWorkspace.Roots().isEmpty());
    ui->migration->setEnabled(allowed && !fWorkspace.Roots().isEmpty() && fWorkspace.DirtyFiles().isEmpty());
}
void ConfigurationWorkspaceWindow::SyncEditors() {
    AddSourceTabs();
    for(auto it=fEditors.constBegin();it!=fEditors.constEnd();++it){const QSignalBlocker blocked(it.value());it.value()->setPlainText(QString::fromUtf8(fWorkspace.Bytes(it.key())));}
    fPendingFiles.clear();InvalidateReview();
}
void ConfigurationWorkspaceWindow::EditStructure() {
    if(fReadOnly || fWorkspace.RequiresReopen() || HasPendingText() || fWorkspace.Roots().isEmpty())return;
    WorkspaceStructureWindow dialog(&fWorkspace,this);
    if(dialog.exec()==QDialog::Accepted){SyncEditors();ui->status->setText(tr("Staged the reviewed structural change in workspace history. Review all roots again before Save All; disk is unchanged."));}
}
void ConfigurationWorkspaceWindow::InspectRecovery() {
    WorkspaceRecoveryWindow dialog(fRecoveryDirectory.isEmpty() ? fDirectory : fRecoveryDirectory,fReadOnly,this);dialog.exec();
}
void ConfigurationWorkspaceWindow::MigrateSource() {
    if(fReadOnly || fWorkspace.RequiresReopen() || HasPendingText() || !fWorkspace.DirtyFiles().isEmpty())return;
    WorkspaceMigrationWindow dialog(&fWorkspace,false,this);const auto answer=dialog.exec();
    const auto result=dialog.Result();if(!result.recoveryDirectory.isEmpty())fRecoveryDirectory=result.recoveryDirectory;
    InvalidateReview();
    if(answer==QDialog::Accepted && result.complete) {
        XcfgWorkspace reopened;QString error;bool okay=true;
        for(const auto &root:result.roots)if(!reopened.Open(root,&error)){okay=false;break;}
        if(okay) {
            for(auto editor:fEditors){editor->disconnect(this);delete editor;}fEditors.clear();fPendingFiles.clear();
            fWorkspace=reopened;AddSourceTabs();RefreshHistory();
            ui->roots->setText(tr("Opened roots:\n%1").arg(fWorkspace.Roots().join("\n")));
            ui->status->setText(tr("Migration completed. Reopened the new source paths with fresh history. Recovery record: %1").arg(result.recoveryDirectory));return;
        }
        ui->status->setText(tr("Migration completed, but reopening failed: %1\nOpen the recorded new roots after inspection.").arg(error));
    }else if(fWorkspace.RequiresReopen())ui->status->setText(tr("Migration requires inspection. Recovery record: %1").arg(fRecoveryDirectory));
    if(fWorkspace.RequiresReopen()) {
        ui->reviewAll->setEnabled(false);ui->openRoot->setEnabled(false);ui->saveAll->setEnabled(false);
        for(auto editor:fEditors)editor->setReadOnly(true);
    }
}
void ConfigurationWorkspaceWindow::Undo() {ApplyHistory(false);}
void ConfigurationWorkspaceWindow::Redo() {ApplyHistory(true);}
void ConfigurationWorkspaceWindow::ApplyHistory(bool redo) {
    if(fReadOnly || fWorkspace.RequiresReopen() || HasPendingText())return;
    QString error;
    if(!(redo ? fWorkspace.Redo(&error) : fWorkspace.Undo(&error))) {InvalidateReview();ui->status->setText(error);return;}
    SyncEditors();
    ui->status->setText(tr("%1 the complete reviewed source group in memory. Review all roots again before Save All; disk files were not changed.")
        .arg(redo ? tr("Redid") : tr("Undid")));
}
void ConfigurationWorkspaceWindow::ReviewAll() {
    if(fReadOnly || fWorkspace.RequiresReopen()) return;
    QMap<QString,QByteArray> changes;
    for(auto it=fEditors.constBegin();it!=fEditors.constEnd();++it) {
        const auto previous=fWorkspace.Bytes(it.key());
        changes[it.key()]=it.value()->toPlainText()==EditorText(previous) ? previous : it.value()->toPlainText().toUtf8();
    }
    // Stage all buffers together: valid combined changes need not be valid one file at a time.
    XcfgWorkspace candidate=fWorkspace;QString error;XcfgWorkspace::Preview preview;
    if(!candidate.StageSources(changes,&error) || !candidate.CreatePreview(&preview,&error)) {
        InvalidateReview();ui->status->setText(error);return;
    }
    fWorkspace=candidate;AddSourceTabs();fPendingFiles.clear();fPreview=preview;fHaveReview=true;
    QJsonObject report;report["hardware_loaded_verified"]=false;report["cross_file_transaction"]=false;
    QJsonArray writes;
    for(auto it=preview.writes.constBegin();it!=preview.writes.constEnd();++it) {
        QJsonObject file;file["path"]=it.key();file["new_bytes"]=QString::number(it->size());writes.append(file);
    }
    report["files_to_replace"]=writes;QJsonObject roots;
    for(auto it=preview.reports.constBegin();it!=preview.reports.constEnd();++it) roots[it.key()]=it.value();
    report["root_reviews"]=roots;ui->review->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented)));
    ui->saveAll->setEnabled(!preview.writes.isEmpty());
    RefreshHistory();
    ui->status->setText(tr("Reviewed %1 changed files across %2 roots. Inspect every root review, then Save All.").arg(preview.writes.size()).arg(fWorkspace.Roots().size()));
}
void ConfigurationWorkspaceWindow::SaveAll() {
    if(fReadOnly || !fHaveReview || HasPendingText()) return;
    XcfgWorkspace::SaveResult result;QString error;
    const bool saved=fWorkspace.SaveAll(fPreview,&result,&error);
    fRecoveryDirectory=result.recoveryDirectory;
    InvalidateReview();
    if(!saved) {
        ui->status->setText(error+tr("\nSaved: %1\nRemaining: %2\nRecovery record: %3").arg(result.savedFiles.join("\n"),result.remainingFiles.join("\n"),result.recoveryDirectory));
        if(fWorkspace.RequiresReopen()) {
            ui->reviewAll->setEnabled(false);ui->openRoot->setEnabled(false);
            for(auto editor:fEditors) editor->setReadOnly(true);
        }
        return;
    }
    ui->status->setText(tr("Saved all %1 reviewed files. Recovery originals/candidate retained at %2. No hardware command was sent.").arg(result.savedFiles.size()).arg(result.recoveryDirectory));
}
void ConfigurationWorkspaceWindow::closeEvent(QCloseEvent *event) {
    if(!fReadOnly && (HasPendingText() || !fWorkspace.DirtyFiles().isEmpty())) {
        const auto answer=QMessageBox::question(this,tr("Unsaved workspace"),
            tr("This workspace has unsaved source text. Close and discard the in-memory changes? Files already saved remain saved."),
            QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel);
        if(answer!=QMessageBox::Discard) {event->ignore();return;}
    }
    QDialog::closeEvent(event);
}
