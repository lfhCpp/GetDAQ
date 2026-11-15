#include "configwindow.h"
#include "xcfgexporter.h"
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QFileDialog>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <memory>

void ConfigWindow::OpenXmlExport()
{
    if(fDocument.RootFile().isEmpty()){fStatus->setText(tr("Open a configuration first."));return;}
    auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("ConfigXmlExportDialog");
    dialog->setWindowTitle(tr("Review self-contained XML export / Save As"));dialog->resize(1000,720);auto layout=new QVBoxLayout(dialog);
    auto explanation=new QLabel(tr("Declarations: keep local/default declarations and expand includes; inherited values from outside the selected declaration are not added.\n"
        "Effective object: freeze the selected concrete object and every effective descendant, including shared units/ranges.\n"
        "Choose the root path for a whole-file copy. The new file must not exist. Source files and hardware are unchanged."),dialog);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto form=new QFormLayout;auto path=new QLineEdit(dialog);path->setObjectName("ConfigXmlExportPath");
    const auto lookup=findChild<QLineEdit *>("ConfigLookupPath");
    path->setText(lookup && !lookup->text().trimmed().isEmpty() ? lookup->text().trimmed() : fDocument.Nodes()[fDocument.Sources().value(fDocument.RootFile()).root].path);
    auto mode=new QComboBox(dialog);mode->setObjectName("ConfigXmlExportMode");mode->addItems({tr("Flatten declarations (keep defaults)"),tr("Current effective concrete object")});
    auto destination=new QLineEdit(dialog);destination->setObjectName("ConfigXmlExportDestination");
    destination->setPlaceholderText(tr("Path of a NEW .xcfg file"));auto browse=new QPushButton(tr("Choose new file..."),dialog);
    form->addRow(tr("Object query path"),path);form->addRow(tr("Export mode"),mode);form->addRow(tr("New output file"),destination);layout->addLayout(form);layout->addWidget(browse);
    auto openSaved=new QCheckBox(tr("After saving, open the new copy in this editor (clears this session's undo/redo history)"),dialog);
    openSaved->setObjectName("ConfigXmlExportOpenSaved");openSaved->setEnabled(!fReadOnly);layout->addWidget(openSaved);
    auto tabs=new QTabWidget(dialog);auto xml=new QPlainTextEdit(dialog);xml->setObjectName("ConfigXmlExportText");xml->setReadOnly(true);
    auto dependencies=new QTreeWidget(dialog);dependencies->setObjectName("ConfigXmlExportSources");dependencies->setHeaderLabels({tr("Source file"),tr("Bytes"),tr("SHA-256")});
    tabs->addTab(xml,tr("Proposed self-contained XML"));tabs->addTab(dependencies,tr("Source guards"));layout->addWidget(tabs,1);
    auto status=new QLabel(dialog);status->setObjectName("ConfigXmlExportStatus");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto actions=new QHBoxLayout;auto previewButton=new QPushButton(tr("Build preview"),dialog);previewButton->setObjectName("ConfigXmlExportPreview");
    auto save=new QPushButton(tr("Save reviewed NEW file"),dialog);save->setObjectName("ConfigXmlExportSave");save->setEnabled(false);auto close=new QPushButton(tr("Close"),dialog);
    actions->addWidget(previewButton);actions->addWidget(save);actions->addWidget(close);layout->addLayout(actions);
    auto proposed=std::make_shared<XcfgExporter::Preview>();auto have=std::make_shared<bool>(false),busy=std::make_shared<bool>(false);
    auto reviewedRevision=std::make_shared<int>(-1);
    const auto clear=[have,save,xml,dependencies] { *have=false;save->setEnabled(false);xml->clear();dependencies->clear(); };
    connect(path,&QLineEdit::textChanged,dialog,clear);connect(destination,&QLineEdit::textChanged,dialog,clear);
    connect(mode,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,clear);
    connect(browse,&QPushButton::clicked,dialog,[this,dialog,destination] {
        const auto chosen=QFileDialog::getSaveFileName(dialog,tr("Choose a NEW XML configuration"),fDirectory,tr("GET XML (*.xcfg *.xml)"),nullptr,QFileDialog::DontConfirmOverwrite);
        if(!chosen.isEmpty())destination->setText(chosen);
    });
    const auto controls=[this,path,mode,destination,browse,openSaved,previewButton,save,busy,have](bool working) {
        *busy=working;path->setEnabled(!working);mode->setEnabled(!working);destination->setEnabled(!working);browse->setEnabled(!working);
        openSaved->setEnabled(!working && !fReadOnly);previewButton->setEnabled(!working);save->setEnabled(!working && *have);
    };
    connect(previewButton,&QPushButton::clicked,dialog,[this,dialog,path,mode,destination,status,proposed,have,reviewedRevision,xml,dependencies,clear,controls,busy] {
        if(*busy)return;
        clear();XcfgExporter::Request request;request.path=path->text().trimmed();request.destination=destination->text();
        request.mode=static_cast<XcfgExporter::Mode>(mode->currentIndex());const int revision=fRevision;const XcfgDocument document=fDocument;
        typedef QPair<XcfgExporter::Preview,QString> Result;auto watcher=new QFutureWatcher<Result>(dialog);controls(true);status->setText(tr("Building XML preview in the background..."));
        connect(watcher,&QFutureWatcher<Result>::finished,dialog,[this,watcher,revision,request,path,mode,destination,status,proposed,have,reviewedRevision,xml,dependencies,controls] {
            const auto result=watcher->result();watcher->deleteLater();
            if(revision!=fRevision || request.path!=path->text().trimmed() || request.destination!=destination->text() || int(request.mode)!=mode->currentIndex()) {
                controls(false);status->setText(tr("Configuration or export inputs changed; build a new preview."));return;
            }
            if(!result.second.isEmpty()){controls(false);status->setText(result.second);return;}
            *proposed=result.first;*have=true;*reviewedRevision=revision;xml->setPlainText(QString::fromUtf8(proposed->xml));
            for(auto it=proposed->sourceBytes.constBegin();it!=proposed->sourceBytes.constEnd();++it)
                new QTreeWidgetItem(dependencies,{it.key(),QString::number(it.value().size()),QString::fromLatin1(QCryptographicHash::hash(it.value(),QCryptographicHash::Sha256).toHex())});
            controls(false);status->setText(tr("Preview only. Output: %1\n%2 objects / %3 scalars; effective SHA-256: %4\nInspect the XML and source guards before saving.")
                .arg(proposed->request.destination).arg(proposed->objectCount).arg(proposed->scalarCount).arg(proposed->effectiveSha256));
        });
        watcher->setFuture(QtConcurrent::run([document,request]() -> Result {XcfgExporter::Preview result;QString error;XcfgExporter::CreatePreview(document,request,&result,&error);return qMakePair(result,error);}));
    });
    connect(save,&QPushButton::clicked,dialog,[this,dialog,status,proposed,have,reviewedRevision,openSaved,controls,busy,clear] {
        if(*busy || !*have)return;
        if(*reviewedRevision!=fRevision){clear();status->setText(tr("Configuration changed; build a new preview."));return;}
        const auto preview=*proposed;const XcfgDocument document=fDocument;const int revision=fRevision;const bool openCopy=openSaved->isChecked();
        auto watcher=new QFutureWatcher<QString>(dialog);controls(true);status->setText(tr("Checking source guards and saving the new XML file..."));
        connect(watcher,&QFutureWatcher<QString>::finished,dialog,[this,watcher,revision,openCopy,preview,status,clear,controls] {
            const auto error=watcher->result();watcher->deleteLater();clear();controls(false);
            if(!error.isEmpty()){status->setText(error);return;}
            QString message=tr("Saved %1. Source files and hardware were unchanged.").arg(preview.request.destination);
            if(openCopy && revision==fRevision) {QString openError;if(Open(preview.request.destination,&openError))message+=tr("\nOpened the new copy; previous editing history cleared.");else message+=tr("\nSaved, but opening the copy failed: ")+openError;}
            else if(revision!=fRevision)message+=tr("\nThis export belongs to the earlier configuration; the current editor was not changed.");
            status->setText(message);fStatus->setText(message);
        });
        watcher->setFuture(QtConcurrent::run([document,preview] {QString error;XcfgExporter::Save(document,preview,&error);return error;}));
    });
    connect(close,&QPushButton::clicked,dialog,&QDialog::close);dialog->show();
}
