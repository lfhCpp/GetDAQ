#include "workspacerecoverywindow.h"
#include "ui_workspacerecoverywindow.h"
#include <QFileDialog>
#include <QJsonDocument>
#include <QMessageBox>
#include <QSignalBlocker>
WorkspaceRecoveryWindow::WorkspaceRecoveryWindow(const QString &directory,bool readOnly,QWidget *parent)
    : QDialog(parent),ui(new Ui::WorkspaceRecoveryWindow),fReadOnly(readOnly) {
    ui->setupUi(this);ui->directory->setText(directory);ui->direction->addItems({tr("Finish the reviewed candidate"),tr("Restore the recorded original sources")});
    connect(ui->choose,&QPushButton::clicked,this,&WorkspaceRecoveryWindow::Choose);connect(ui->directory,&QLineEdit::textChanged,this,&WorkspaceRecoveryWindow::Invalidate);connect(ui->direction,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceRecoveryWindow::Invalidate);connect(ui->source,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceRecoveryWindow::ShowSource);connect(ui->preview,&QPushButton::clicked,this,&WorkspaceRecoveryWindow::Preview);connect(ui->apply,&QPushButton::clicked,this,&WorkspaceRecoveryWindow::Apply);
}
WorkspaceRecoveryWindow::~WorkspaceRecoveryWindow(){delete ui;}
void WorkspaceRecoveryWindow::Choose(){const auto directory=QFileDialog::getExistingDirectory(this,tr("Select a save-* recovery record"),ui->directory->text());if(!directory.isEmpty())ui->directory->setText(directory);}
void WorkspaceRecoveryWindow::Invalidate(){++fReviewGeneration;fHavePreview=false;ui->apply->setEnabled(false);ui->review->clear();ui->source->clear();ui->current->clear();ui->target->clear();ui->status->setText(tr("Preview the selected record and direction; no file has been written."));}
void WorkspaceRecoveryWindow::ShowSource(){
    ui->current->clear();ui->target->clear();if(!fHavePreview)return;const auto path=ui->source->currentText();
    if(fMigrationRecord) {
        ui->current->setPlainText(fMigration.missingObserved.contains(path) ? tr("[This path is currently absent]") : QString::fromUtf8(fMigration.observed.value(path)));
        ui->target->setPlainText(fMigration.missingTarget.contains(path) ? tr("[This path will be absent after recovery]") : QString::fromUtf8(fMigration.target.value(path)));
    }else {ui->current->setPlainText(QString::fromUtf8(fPreview.observed.value(path)));ui->target->setPlainText(QString::fromUtf8(fPreview.target.value(path)));}
}
void WorkspaceRecoveryWindow::Preview(){
    Invalidate();QString error;fMigrationRecord=XcfgSourceMigration::IsMigrationRecord(ui->directory->text());QStringList paths;QJsonObject report;
    if(fMigrationRecord) {
        if(!XcfgSourceMigration::PreviewRecovery(ui->directory->text(),ui->direction->currentIndex()==0,&fMigration,&error)){ui->status->setText(error);return;}
        paths=fMigration.observed.keys();paths.append(fMigration.missingObserved);paths.sort();report=fMigration.report;
    }else {
        if(!XcfgSaveJournal::PreviewRecovery(ui->directory->text(),ui->direction->currentIndex()==0,&fPreview,&error)){ui->status->setText(error);return;}
        paths=fPreview.observed.keys();report=fPreview.report;
    }
    ui->review->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson()));fHavePreview=true;
    {const QSignalBlocker blocked(ui->source);ui->source->addItems(paths);}ShowSource();ui->apply->setEnabled(!fReadOnly);
    ui->status->setText(fReadOnly ? tr("Read-only mode: review allowed, recovery writes disabled.") :
        tr("Inspect every source and selected direction. A path migration may create or remove recorded paths; unrelated changes are refused."));
}
void WorkspaceRecoveryWindow::Apply(){
    if(fReadOnly || !fHavePreview)return;
    const auto generation=fReviewGeneration;
    const auto answer=QMessageBox::question(this,tr("Recover interrupted configuration save"),
        tr("%1 for %2 recorded source paths?\nPath migration recovery can create or remove the listed paths. This rechecks the exact review and sends no hardware command. Reopen the workspace after success.")
        .arg(ui->direction->currentText()).arg(ui->source->count()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel);
    if(answer!=QMessageBox::Yes || !fHavePreview || generation!=fReviewGeneration)return;
    QString error;
    const bool okay=fMigrationRecord ? XcfgSourceMigration::ApplyRecovery(fMigration,&error) : XcfgSaveJournal::ApplyRecovery(fPreview,&error);
    Invalidate();ui->status->setText(okay ? tr("Recovery completed and recorded. Reopen the recorded roots; old windows and history are not rebound automatically.") :
        error+tr("\nIntent retained. Inspect a new review; no automatic retry is scheduled."));
}
