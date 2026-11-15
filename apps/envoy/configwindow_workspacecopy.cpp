#include "configwindow.h"
#include "xcfgworkspacecopy.h"
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>
#include <memory>

void ConfigWindow::OpenWorkspaceCopy()
{
    if(fDocument.RootFile().isEmpty()){fStatus->setText(tr("Open a configuration first."));return;}
    auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("ConfigWorkspaceCopyDialog");dialog->resize(1000,720);
    dialog->setWindowTitle(tr("Review editable configuration workspace copy"));auto layout=new QVBoxLayout(dialog);
    auto explanation=new QLabel(tr("Copy the main XML and every loaded include into a NEW directory. Original files stay unchanged.\n"
        "Shared mode keeps repeated references linked to one copied file. Isolated mode gives each use its own file, so later source edits can affect one location.\n"
        "The copied XML remains editable; the inventory records its initial state and is not a hardware publication."),dialog);
    explanation->setWordWrap(true);layout->addWidget(explanation);
    auto form=new QFormLayout;auto mode=new QComboBox(dialog);mode->setObjectName("ConfigWorkspaceCopyMode");
    mode->addItems({tr("Keep shared source files"),tr("Isolate each include reference")});mode->setCurrentIndex(1);
    auto directory=new QLineEdit(dialog);directory->setObjectName("ConfigWorkspaceCopyDirectory");directory->setPlaceholderText(tr("Path of a NEW directory; its parent must exist"));
    auto openSaved=new QCheckBox(tr("Open the copied main.xcfg after saving (clears this editor's undo/redo history)"),dialog);openSaved->setObjectName("ConfigWorkspaceCopyOpenSaved");openSaved->setEnabled(!fReadOnly);
    form->addRow(tr("Include sharing"),mode);form->addRow(tr("New workspace directory"),directory);layout->addLayout(form);layout->addWidget(openSaved);
    auto tabs=new QTabWidget(dialog);auto files=new QTreeWidget(dialog);files->setObjectName("ConfigWorkspaceCopyFiles");files->setHeaderLabels({tr("Copied file"),tr("Original source"),tr("Bytes"),tr("Output SHA-256")});
    auto inventory=new QPlainTextEdit(dialog);inventory->setObjectName("ConfigWorkspaceCopyInventory");inventory->setReadOnly(true);
    auto mainXml=new QPlainTextEdit(dialog);mainXml->setObjectName("ConfigWorkspaceCopyMainXml");mainXml->setReadOnly(true);
    tabs->addTab(files,tr("Source mapping"));tabs->addTab(inventory,tr("Initial inventory / include edges"));tabs->addTab(mainXml,tr("Copied main XML"));layout->addWidget(tabs,1);
    auto status=new QLabel(dialog);status->setObjectName("ConfigWorkspaceCopyStatus");status->setWordWrap(true);status->setTextFormat(Qt::PlainText);layout->addWidget(status);
    auto actions=new QHBoxLayout;auto preview=new QPushButton(tr("Build preview"),dialog);preview->setObjectName("ConfigWorkspaceCopyPreview");
    auto save=new QPushButton(tr("Create reviewed NEW workspace"),dialog);save->setObjectName("ConfigWorkspaceCopySave");save->setEnabled(false);auto close=new QPushButton(tr("Close"),dialog);
    actions->addWidget(preview);actions->addWidget(save);actions->addWidget(close);layout->addLayout(actions);
    auto proposed=std::make_shared<XcfgWorkspaceCopy::Preview>();auto have=std::make_shared<bool>(false),busy=std::make_shared<bool>(false);auto revision=std::make_shared<int>(-1);
    const auto clear=[have,save,files,inventory,mainXml] {*have=false;save->setEnabled(false);files->clear();inventory->clear();mainXml->clear();};
    const auto controls=[this,mode,directory,openSaved,preview,save,busy,have](bool working) {*busy=working;mode->setEnabled(!working);directory->setEnabled(!working);openSaved->setEnabled(!working && !fReadOnly);preview->setEnabled(!working);save->setEnabled(!working && *have);};
    connect(mode,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,clear);connect(directory,&QLineEdit::textChanged,dialog,clear);
    connect(preview,&QPushButton::clicked,dialog,[this,dialog,mode,directory,proposed,have,busy,revision,files,inventory,mainXml,status,clear,controls] {
        if(*busy)return;
        clear();XcfgWorkspaceCopy::Request request;request.sharing=static_cast<XcfgWorkspaceCopy::Sharing>(mode->currentIndex());request.directory=directory->text();
        const XcfgDocument document=fDocument;const int started=fRevision;typedef QPair<XcfgWorkspaceCopy::Preview,QString> Result;
        auto watcher=new QFutureWatcher<Result>(dialog);controls(true);status->setText(tr("Checking source graph and copied XML in the background..."));
        connect(watcher,&QFutureWatcher<Result>::finished,dialog,[this,watcher,request,started,mode,directory,proposed,have,revision,files,inventory,mainXml,status,controls] {
            const auto result=watcher->result();watcher->deleteLater();
            if(started!=fRevision || request.directory!=directory->text() || int(request.sharing)!=mode->currentIndex()){controls(false);status->setText(tr("Configuration or workspace inputs changed; build a new preview."));return;}
            if(!result.second.isEmpty()){controls(false);status->setText(result.second);return;}
            *proposed=result.first;*have=true;*revision=started;
            for(const auto &item:proposed->inventory.value("files").toArray()) {
                const auto entry=item.toObject();new QTreeWidgetItem(files,{entry.value("stored").toString(),entry.value("source_file").toString(),entry.value("output_bytes").toString(),entry.value("output_sha256").toString()});
            }
            inventory->setPlainText(QString::fromUtf8(QJsonDocument(proposed->inventory).toJson(QJsonDocument::Indented)));mainXml->setPlainText(QString::fromUtf8(proposed->files.value("main.xcfg")));
            controls(false);status->setText(tr("Preview only; target directory not created.\n%1 copied files; effective configuration unchanged: %2\nReview every include mapping before creating the workspace.").arg(proposed->files.size()).arg(proposed->effectiveSha256));
        });
        watcher->setFuture(QtConcurrent::run([document,request]() -> Result {XcfgWorkspaceCopy::Preview result;QString error;XcfgWorkspaceCopy::CreatePreview(document,request,&result,&error);return qMakePair(result,error);}));
    });
    connect(save,&QPushButton::clicked,dialog,[this,dialog,proposed,have,busy,revision,openSaved,status,clear,controls] {
        if(*busy || !*have)return;
        if(*revision!=fRevision){clear();status->setText(tr("Configuration changed; review a new workspace preview."));return;}
        const auto proposal=*proposed;const XcfgDocument document=fDocument;const int started=fRevision;const bool openCopy=openSaved->isChecked();
        typedef QPair<QJsonObject,QString> Result;auto watcher=new QFutureWatcher<Result>(dialog);controls(true);status->setText(tr("Creating and verifying the copied workspace..."));
        connect(watcher,&QFutureWatcher<Result>::finished,dialog,[this,watcher,started,openCopy,status,clear,controls] {
            const auto result=watcher->result();watcher->deleteLater();clear();controls(false);
            if(!result.second.isEmpty()){status->setText(result.second);return;}
            QString message=tr("Created editable workspace: %1\nOriginal files and hardware remain unchanged.").arg(result.first.value("directory").toString());
            if(openCopy && started==fRevision){QString error;if(Open(result.first.value("root_file").toString(),&error))message+=tr("\nOpened copied main.xcfg; old editing history cleared.");else message+=tr("\nCreated, but opening failed: ")+error;}
            else if(started!=fRevision)message+=tr("\nThis copy belongs to the earlier document; current editor was not changed.");
            status->setText(message);fStatus->setText(message);
        });
        watcher->setFuture(QtConcurrent::run([document,proposal]() -> Result {QJsonObject result;QString error;XcfgWorkspaceCopy::Save(document,proposal,&result,&error);return qMakePair(result,error);}));
    });
    connect(close,&QPushButton::clicked,dialog,&QDialog::close);dialog->show();
}
