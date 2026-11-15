#include "workspacemigrationwindow.h"
#include "ui_workspacemigrationwindow.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSignalBlocker>

WorkspaceMigrationWindow::WorkspaceMigrationWindow(XcfgWorkspace *workspace,bool readOnly,QWidget *parent)
    : QDialog(parent),ui(new Ui::WorkspaceMigrationWindow),fWorkspace(workspace),fReadOnly(readOnly) {
    ui->setupUi(this);
    if(fWorkspace)ui->source->addItems(fWorkspace->Files());
    connect(ui->source,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceMigrationWindow::Invalidate);
    connect(ui->destination,&QLineEdit::textChanged,this,&WorkspaceMigrationWindow::Invalidate);
    connect(ui->choose,&QPushButton::clicked,this,&WorkspaceMigrationWindow::ChooseDestination);
    connect(ui->preview,&QPushButton::clicked,this,&WorkspaceMigrationWindow::Preview);
    connect(ui->apply,&QPushButton::clicked,this,&WorkspaceMigrationWindow::Apply);
    connect(ui->reviewSource,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceMigrationWindow::ShowSource);
    Invalidate();
}
WorkspaceMigrationWindow::~WorkspaceMigrationWindow(){delete ui;}
void WorkspaceMigrationWindow::ChooseDestination() {
    const auto path=QFileDialog::getSaveFileName(this,tr("Choose an absent destination in an existing directory"),
        QFileInfo(ui->source->currentText()).absolutePath(),tr("GET XML (*.xcfg *.xml);;All files (*)"));
    if(!path.isEmpty())ui->destination->setText(path);
}
void WorkspaceMigrationWindow::Invalidate() {
    ++fReviewGeneration;
    fHavePreview=false;ui->apply->setEnabled(false);ui->review->clear();ui->reviewSource->clear();ui->before->clear();ui->after->clear();
    ui->status->setText(tr("Preview the new path and every opened root. No configuration file has been written."));
}
void WorkspaceMigrationWindow::Preview() {
    Invalidate();if(!fWorkspace)return;
    XcfgSourceMigration::Request request;request.destinations[ui->source->currentText()]=ui->destination->text();QString error;
    if(!XcfgSourceMigration::CreatePreview(*fWorkspace,request,&fPreview,&error)){ui->status->setText(error);return;}
    ui->review->setPlainText(QString::fromUtf8(QJsonDocument(fPreview.report).toJson(QJsonDocument::Indented)));
    fHavePreview=true;ui->apply->setEnabled(!fReadOnly);
    auto paths=fPreview.original.keys();for(const auto &path:fPreview.candidate.keys())if(!paths.contains(path))paths.append(path);paths.sort();
    {const QSignalBlocker blocked(ui->reviewSource);ui->reviewSource->addItems(paths);}ShowSource();
    ui->status->setText(fReadOnly ? tr("Read-only mode: preview allowed, migration disabled.") :
        tr("Review the source, absent destination, include rewrites and all roots. Other unopened projects may still reference the old path."));
}
void WorkspaceMigrationWindow::ShowSource() {
    ui->before->clear();ui->after->clear();if(!fHavePreview)return;const auto path=ui->reviewSource->currentText();
    ui->before->setPlainText(fPreview.original.contains(path) ? QString::fromUtf8(fPreview.original.value(path)) : tr("[This path does not exist before migration]"));
    ui->after->setPlainText(fPreview.candidate.contains(path) ? QString::fromUtf8(fPreview.candidate.value(path)) : tr("[This path will be removed after migration]"));
}
void WorkspaceMigrationWindow::Apply() {
    if(fReadOnly || !fWorkspace || !fHavePreview)return;
    const auto generation=fReviewGeneration;
    const auto answer=QMessageBox::question(this,tr("Migrate configuration source path"),
        tr("Move %1 to %2 and rewrite references in all %3 opened roots?\nThe old source will be removed after new files and references are saved. This does not search unopened projects or send hardware commands.")
        .arg(ui->source->currentText(),ui->destination->text()).arg(fWorkspace->Roots().size()),
        QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
    if(answer!=QMessageBox::Yes || !fHavePreview || generation!=fReviewGeneration)return;
    QString error;const bool okay=XcfgSourceMigration::Apply(fWorkspace,fPreview,&fResult,&error);Invalidate();
    if(okay){accept();return;}
    ui->status->setText(error+tr("\nRecovery record: %1\nIf a record was created, inspect it before reopening. No automatic retry is scheduled.").arg(fResult.recoveryDirectory));
}
