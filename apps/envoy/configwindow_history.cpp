#include "configwindow.h"
#include <QDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>

void ConfigWindow::UpdateHistoryButtons()
{
    fUndo->setEnabled(!fReadOnly && fHistory.UndoSteps()>0);
    fRedo->setEnabled(!fReadOnly && fHistory.RedoSteps()>0);
}
void ConfigWindow::OpenHistoryReview(bool redo)
{
    if(fReadOnly) return;
    auto preview=std::make_shared<XcfgEditHistory::Preview>();QString error;
    if(!fHistory.CreatePreview(fDocument,redo,preview.get(),&error)) {fStatus->setText(error);return;}
    const int revision=fRevision;
    auto dialog=new QDialog(this);dialog->setObjectName("ConfigHistoryDialog");dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(redo ? tr("Review local redo") : tr("Review local undo"));dialog->resize(1000,650);
    auto layout=new QVBoxLayout(dialog);
    auto summary=new QLabel(tr("%1: %2\nSource: %3\nReview all effective changes before saving. This changes one local file and sends no hardware command.")
        .arg(redo ? tr("Redo") : tr("Undo"),preview->label,preview->destinationFile),dialog);
    summary->setWordWrap(true);layout->addWidget(summary);
    auto table=new QTreeWidget(dialog);table->setObjectName("ConfigHistoryChanges");
    table->setHeaderLabels({tr("Path"),tr("Change"),tr("Before"),tr("After")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);table->setMaximumHeight(200);layout->addWidget(table);
    const auto describe=[](const QJsonObject &value) {return value.value("value").toString()+" ["+value.value("type").toString()+"] "+
        value.value("unit").toString()+" "+value.value("range").toString();};
    for(const auto &item:preview->report.value("scalar_changes").toArray()) {
        const auto entry=item.toObject();new QTreeWidgetItem(table,{entry.value("path").toString(),entry.value("change").toString(),
            describe(entry.value("before").toObject()),describe(entry.value("after").toObject())});
    }
    for(const auto &item:preview->report.value("object_changes").toArray()) {
        const auto entry=item.toObject();new QTreeWidgetItem(table,{entry.value("path").toString(),tr("Object: %1").arg(entry.value("change").toString())});
    }
    auto tabs=new QTabWidget(dialog);layout->addWidget(tabs,1);
    auto original=new QPlainTextEdit(dialog);original->setObjectName("ConfigHistoryOriginal");original->setReadOnly(true);
    original->setPlainText(QString::fromUtf8(preview->originalBytes));tabs->addTab(original,tr("Current XML"));
    auto updated=new QPlainTextEdit(dialog);updated->setObjectName("ConfigHistoryUpdated");updated->setReadOnly(true);
    updated->setPlainText(QString::fromUtf8(preview->updatedBytes));tabs->addTab(updated,tr("Proposed XML"));
    auto report=new QPlainTextEdit(dialog);report->setObjectName("ConfigHistoryReport");report->setReadOnly(true);
    report->setPlainText(QString::fromUtf8(QJsonDocument(preview->report).toJson(QJsonDocument::Indented)));tabs->addTab(report,tr("Full effective/source review"));
    auto status=new QLabel(dialog);status->setObjectName("ConfigHistoryStatus");status->setWordWrap(true);layout->addWidget(status);
    auto apply=new QPushButton(tr("Apply reviewed %1").arg(redo ? tr("redo") : tr("undo")),dialog);
    apply->setObjectName("ConfigHistoryApply");layout->addWidget(apply);
    connect(apply,&QPushButton::clicked,dialog,[this,revision,preview,apply,status,dialog] {
        if(fReadOnly || !apply->isEnabled()) return;
        apply->setEnabled(false);
        if(revision!=fRevision) {status->setText(tr("Configuration changed; close this dialog and create a new review."));return;}
        QString error;if(!fHistory.Apply(&fDocument,*preview,&error)) {UpdateHistoryButtons();if(fDocument.Nodes().isEmpty()) Rebuild();status->setText(error);return;}
        Rebuild();fStatus->setText(tr("Saved reviewed local %1 to %2. No hardware command was sent.")
            .arg(preview->redo ? tr("redo") : tr("undo"),preview->destinationFile));dialog->close();
    });
    dialog->show();
}
