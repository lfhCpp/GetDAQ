#include "configwindow.h"
#include "configvaluedelegate_p.h"
#include "configurationworkspacewindow.h"
#include "hardwarecompiler.h"
#include "effectiveconfigurationsnapshot.h"
#include "systemconfigurationpublisher.h"
#include "configurationrepositorywindow.h"
#include "xcfgexporter.h"
#include "binaryconfigurationwindow.h"
#include "hardwaredescriptionwindow.h"
#include <QHBoxLayout>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTimer>

ConfigWindow::ConfigWindow(const QString &directory, bool readOnly, QWidget *parent)
    : QDialog(parent), fDirectory(directory), fReadOnly(readOnly)
{
    setWindowTitle(tr("GET configuration files")); resize(1100, 700);
    auto layout = new QVBoxLayout(this);
    auto description = new QLabel(tr("Local .xcfg files and includes. Values, units and ranges shown here are declarations in each source file.\n"
        "Find value source also resolves shared units/ranges and shows their declarations. Hardware validity and deployment require separate checks. Changes stay on this computer."), this);
    description->setWordWrap(true); layout->addWidget(description);
    auto pages=new QTabWidget(this);pages->setObjectName("ConfigPages");layout->addWidget(pages,1);
    auto editPage=new QWidget(pages);auto editLayout=new QVBoxLayout(editPage);editLayout->setContentsMargins(6,6,6,6);
    auto toolsPage=new QWidget(pages);auto toolsLayout=new QVBoxLayout(toolsPage);toolsLayout->setContentsMargins(6,6,6,6);
    pages->addTab(editPage,tr("Edit configuration"));pages->addTab(toolsPage,tr("Configuration tools"));
    auto open = fOpenButton = new QPushButton(tr("Open .xcfg..."), this); editLayout->addWidget(open);
    auto multiWorkspace=new QPushButton(tr("Multi-document workspace / Save All..."),this);
    multiWorkspace->setObjectName("ConfigMultiWorkspace");toolsLayout->addWidget(multiWorkspace);
    connect(multiWorkspace,&QPushButton::clicked,this,[this] {
        auto window=new ConfigurationWorkspaceWindow(fDirectory,fReadOnly,this);
        window->setAttribute(Qt::WA_DeleteOnClose);window->show();
    });
    auto exportXml=new QPushButton(tr("Review XML export / Save As..."),this);exportXml->setObjectName("ConfigExportXml");toolsLayout->addWidget(exportXml);
    connect(exportXml,&QPushButton::clicked,this,&ConfigWindow::OpenXmlExport);
    auto workspaceCopy=new QPushButton(tr("Review source workspace copy..."),this);workspaceCopy->setObjectName("ConfigWorkspaceCopy");toolsLayout->addWidget(workspaceCopy);
    connect(workspaceCopy,&QPushButton::clicked,this,&ConfigWindow::OpenWorkspaceCopy);
    auto binary=new QPushButton(tr("Review legacy binary configuration conversion..."),this);binary->setObjectName("ConfigBinaryConversion");toolsLayout->addWidget(binary);
    connect(binary,&QPushButton::clicked,this,[this]{auto window=new BinaryConfigurationWindow(fDocument,fDirectory,this);window->setAttribute(Qt::WA_DeleteOnClose);window->show();});
    auto historyRow=new QHBoxLayout;
    fUndo=new QPushButton(tr("Review undo..."),this);fUndo->setObjectName("ConfigUndo");
    fRedo=new QPushButton(tr("Review redo..."),this);fRedo->setObjectName("ConfigRedo");
    historyRow->addWidget(fUndo);historyRow->addWidget(fRedo);editLayout->addLayout(historyRow);
    connect(fUndo,&QPushButton::clicked,this,[this] {OpenHistoryReview(false);});
    connect(fRedo,&QPushButton::clicked,this,[this] {OpenHistoryReview(true);});
    UpdateHistoryButtons();
    fStructureButton=new QPushButton(tr("Edit selected declaration structure..."),this);
    fStructureButton->setObjectName("ConfigStructureOpen");fStructureButton->setEnabled(false);editLayout->addWidget(fStructureButton);
    connect(fStructureButton,&QPushButton::clicked,this,&ConfigWindow::OpenStructureEditor);
    auto repository=new QPushButton(tr("Remote configuration repository..."),this);repository->setObjectName("OpenConfigurationRepository");toolsLayout->addWidget(repository);
    connect(repository,&QPushButton::clicked,this,[this] {auto window=new ConfigurationRepositoryWindow(this);window->setAttribute(Qt::WA_DeleteOnClose);window->show();});
    auto exportHardware = new QPushButton(tr("Export hardware description..."), this);
    exportHardware->setObjectName("ExportHardwareDescription"); toolsLayout->addWidget(exportHardware);
    auto viewHardware=new QPushButton(tr("View compiled hardware description..."),this);viewHardware->setObjectName("ViewHardwareDescription");toolsLayout->addWidget(viewHardware);
    connect(viewHardware,&QPushButton::clicked,this,[this] {
        const auto path=QFileDialog::getOpenFileName(this,tr("Choose compiled hardware description"),fDirectory,tr("Hardware description (*.json)"));if (path.isEmpty()) return;
        auto dialog=new HardwareDescriptionWindow(this);QString error;
        if (!dialog->Open(path,&error)) {delete dialog;fStatus->setText(error);return;}
        dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->show();
    });
    auto snapshotRow=new QHBoxLayout;
    auto exportSnapshot=new QPushButton(tr("Export effective snapshot..."),this); exportSnapshot->setObjectName("ConfigSnapshotExport");
    auto compareSnapshot=new QPushButton(tr("Compare with saved snapshot..."),this); compareSnapshot->setObjectName("ConfigSnapshotCompare");
    snapshotRow->addWidget(exportSnapshot); snapshotRow->addWidget(compareSnapshot); toolsLayout->addLayout(snapshotRow);
    auto publish=new QPushButton(tr("Build configuration publication..."),this); publish->setObjectName("ConfigPublicationBuild"); toolsLayout->addWidget(publish);
    connect(publish,&QPushButton::clicked,this,[this,publish] {
        const auto profile=QFileDialog::getOpenFileName(this,tr("Choose reviewed system profile"),fDirectory,tr("System profile (*.json)")); if (profile.isEmpty()) return;
        const auto directory=QFileDialog::getSaveFileName(this,tr("Name a NEW publication directory"),fDirectory,QString(),nullptr,QFileDialog::DontConfirmOverwrite); if (directory.isEmpty()) return;
        publish->setEnabled(false); fStatus->setText(tr("Building a local configuration publication in the background..."));
        typedef QPair<QJsonObject,QString> Result; auto watcher=new QFutureWatcher<Result>(this); const int revision=fRevision; const XcfgDocument document=fDocument;
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,publish,watcher,revision] {
            const auto result=watcher->result(); watcher->deleteLater(); publish->setEnabled(true);
            if (!result.second.isEmpty()) { fStatus->setText(result.second); return; }
            fStatus->setText(tr("Built local publication: %1\nManifest SHA-256: %2\n%3Hardware loading is not verified.")
                .arg(result.first.value("manifest_file").toString(),result.first.value("manifest_sha256").toString(),revision==fRevision ? QString() : tr("This publication belongs to the earlier loaded configuration.\n")));
        });
        watcher->setFuture(QtConcurrent::run([document,profile,directory]() -> Result {
            QJsonObject result; QString error; SystemConfigurationPublisher::Publish(document,profile,directory,&result,&error); return qMakePair(result,error);
        }));
    });
    fSnapshotReview=new QPlainTextEdit(this); fSnapshotReview->setObjectName("ConfigSnapshotReview"); fSnapshotReview->setReadOnly(true); fSnapshotReview->setMaximumHeight(220); fSnapshotReview->hide(); toolsLayout->addWidget(fSnapshotReview);
    connect(exportSnapshot,&QPushButton::clicked,this,[this,exportSnapshot,compareSnapshot] {
        const auto path=QFileDialog::getSaveFileName(this,tr("Save effective snapshot"),fDirectory,tr("Effective snapshot (*.json)"));
        if (path.isEmpty()) return;
        StartSnapshotTask(path,false,exportSnapshot,compareSnapshot);
    });
    connect(compareSnapshot,&QPushButton::clicked,this,[this,exportSnapshot,compareSnapshot] {
        const auto path=QFileDialog::getOpenFileName(this,tr("Choose baseline snapshot"),fDirectory,tr("Effective snapshot (*.json)"));
        if (path.isEmpty()) return;
        StartSnapshotTask(path,true,exportSnapshot,compareSnapshot);
    });
    toolsLayout->addStretch(1);
    fTree = new QTreeWidget(this); fTree->setObjectName("ConfigTree");
    fTree->setHeaderLabels({tr("Element / ID"), tr("Declared value"), tr("Declared unit"), tr("Declared range"), tr("Source line / include")});
    fTree->header()->setSectionResizeMode(QHeaderView::Interactive); fTree->setColumnWidth(0, 290); fTree->setColumnWidth(1, 200);
    fTree->setEditTriggers(fReadOnly ? QAbstractItemView::NoEditTriggers : QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);fTree->setMinimumHeight(260);editLayout->addWidget(fTree, 4);
    auto valueDelegate=new ConfigValueDelegate(&fDocument,[this](int id,const QString &value,QString *error) {
        if (fReadOnly || fValueEditPending || id<0 || id>=fDocument.Nodes().size()) { *error=tr("This declaration is read-only or another value save is in progress.");return false; }
        if(value==fDocument.Nodes()[id].value)return true;
        return SaveValueAsync(id,value);
    },fTree);
    valueDelegate->setErrorHandler([this](const QString &error) { fStatus->setText(error); });
    fTree->setItemDelegateForColumn(1,valueDelegate);
    fSelectionInfo=new QLabel(tr("Open a configuration, then select a scalar parameter to edit its value."),editPage);
    fSelectionInfo->setObjectName("ConfigSelectionInfo");fSelectionInfo->setWordWrap(true);editLayout->addWidget(fSelectionInfo);
    auto lookupRow = new QHBoxLayout;
    auto lookupPath = new QLineEdit(this); lookupPath->setPlaceholderText(tr("Lookup: Setup/Node[CoBo]/Instance[0]/Module/readoutDepth"));
    lookupPath->setObjectName("ConfigLookupPath");
    auto lookupButton = new QPushButton(tr("Find value source"), this);
    lookupButton->setObjectName("ConfigLookupButton");
    lookupRow->addWidget(lookupPath, 1); lookupRow->addWidget(lookupButton); editLayout->addLayout(lookupRow);
    auto row = new QHBoxLayout;
    fValue = new QLineEdit(this); fValue->setObjectName("ConfigValue"); fApply = new QPushButton(tr("Save selected declaration"), this);
    fApply->setObjectName("ConfigSaveValue");
    fEditValue=new QPushButton(tr("Edit value..."),this);fEditValue->setObjectName("ConfigEditValue");
    row->addWidget(fValue, 1); row->addWidget(fApply);row->addWidget(fEditValue); editLayout->addLayout(row);
    connect(fEditValue,&QPushButton::clicked,this,&ConfigWindow::OpenValueEditor);
    auto overrideRow=new QHBoxLayout;
    auto overrideValue=new QLineEdit(this); overrideValue->setObjectName("ConfigOverrideValue"); overrideValue->setPlaceholderText(tr("Value for the concrete lookup path only"));
    auto overridePreview=new QPushButton(tr("Preview instance override"),this); overridePreview->setObjectName("ConfigOverridePreview");
    fOverrideApply=new QPushButton(tr("Apply previewed override"),this); fOverrideApply->setObjectName("ConfigOverrideApply");
    overrideRow->addWidget(overrideValue,1); overrideRow->addWidget(overridePreview); overrideRow->addWidget(fOverrideApply); editLayout->addLayout(overrideRow);
    fOverrideText=new QPlainTextEdit(this); fOverrideText->setObjectName("ConfigOverrideText"); fOverrideText->setReadOnly(true); fOverrideText->setMaximumHeight(180); fOverrideText->hide(); editLayout->addWidget(fOverrideText);
    overrideValue->setEnabled(!fReadOnly); overridePreview->setEnabled(!fReadOnly); fOverrideApply->setEnabled(false);
    fStatus = new QLabel(this); fStatus->setWordWrap(true); editLayout->addWidget(fStatus);
    fStatus->setObjectName("ConfigStatus"); fStatus->setTextFormat(Qt::PlainText);
    fStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    connect(lookupPath,&QLineEdit::textChanged,this,[this] { ClearOverride(); });
    connect(overrideValue,&QLineEdit::textChanged,this,[this] { ClearOverride(); });
    connect(overridePreview,&QPushButton::clicked,this,[this,lookupPath,overrideValue] {
        if (fReadOnly) return;
        ClearOverride(); QString error;
        if (!fDocument.PreviewOverride(lookupPath->text().trimmed(),overrideValue->text(),&fOverride,&error)) { fStatus->setText(error); return; }
        fHaveOverride=true; fOverrideApply->setEnabled(true); fOverrideText->setPlainText(QString::fromUtf8(fOverride.updatedBytes));fOverrideText->show();
        fStatus->setText(tr("Preview only; no file written.\nTarget: %1\nFile: %2\nPrevious value: %3\nOverride value: %4\nOther effective values and shared properties are unchanged. Inspect the proposed XML before applying.")
            .arg(fOverride.path,fOverride.destinationFile,fOverride.previousValue,fOverride.value));
    });
    connect(fOverrideApply,&QPushButton::clicked,this,[this] {
        if (fReadOnly || !fHaveOverride) return;
        QString error; const bool saved=fHistory.ApplyOverride(&fDocument,fOverride,&error); ClearOverride();UpdateHistoryButtons();
        if (!saved) { if(fDocument.Nodes().isEmpty()) Rebuild();fStatus->setText(error); return; }
        Rebuild(); fStatus->setText(tr("Saved the concrete instance override; templates/includes remain unchanged. No hardware command was sent."));
    });
    connect(lookupButton, &QPushButton::clicked, this, [this, lookupPath]() {
        QString error; const int id = fDocument.Lookup(lookupPath->text().trimmed(), &error);
        if (id < 0) { fStatus->setText(error); return; }
        auto node = fDocument.Nodes()[id]; XcfgDocument::ScalarProperties properties;
        if (node.scalar && !fDocument.ResolveScalar(lookupPath->text().trimmed(),&node,&properties,&error)) { fStatus->setText(error); return; }
        for(QTreeWidgetItemIterator it(fTree);*it;++it)if((*it)->data(0,Qt::UserRole).toInt()==id){
            for(auto parent=(*it)->parent();parent;parent=parent->parent())parent->setExpanded(true);
            fTree->setCurrentItem(*it);fTree->scrollToItem(*it);break;
        }
        QString description=tr("Value: %1\nSource: %2:%3 — %4").arg(node.scalar ? node.value : tr("(object)")).arg(node.file).arg(node.line).arg(node.path);
        if (node.scalar) {
            description+=tr("\nShared class: %1\nEffective unit: %2\nEffective range: %3").arg(properties.classPath,properties.unit,properties.range);
            const auto origin=[this](int index) -> QString { if (index<0) return tr("Not declared"); const auto &n=fDocument.Nodes()[index]; return n.file+":"+QString::number(n.line)+" — "+n.path; };
            description+=tr("\nUnit source: %1\nRange source: %2").arg(origin(properties.unitSource),origin(properties.rangeSource));
        }
        fStatus->setText(description);
    });
    if (fReadOnly) fStatus->setText(tr("Read-only while the console is connected. Disconnect before editing local hardware configuration."));
    connect(open, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(this, tr("GET configuration"), fDirectory, tr("GET XML (*.xcfg *.xml)"));
        if (path.isEmpty()) return;
        QString error;
        if (!Open(path, &error)) QMessageBox::warning(this, tr("Configuration"), error);
    });
    connect(fTree, &QTreeWidget::itemSelectionChanged, this, &ConfigWindow::SelectionChanged);
    connect(fTree,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item,int column) {
        if(!item)return;
        const int id=item->data(0,Qt::UserRole).toInt();
        if(id<0 || id>=fDocument.Nodes().size())return;
        if(column==0 && !fDocument.Nodes()[id].scalar)OpenStructureEditor();
    });
    connect(exportHardware, &QPushButton::clicked, this, [this, exportHardware]() {
        if (fDocument.RootFile().isEmpty()) { fStatus->setText(tr("Open a hardware description first.")); return; }
        const QString path = QFileDialog::getSaveFileName(this, tr("Export compiled hardware description"), fDirectory, tr("Hardware description (*.json)"));
        if (path.isEmpty()) return;
        exportHardware->setEnabled(false); fStatus->setText(tr("Compiling the loaded hardware description. No hardware commands are sent."));
        auto watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, exportHardware, watcher, path]() {
            const QString error = watcher->result(); watcher->deleteLater(); exportHardware->setEnabled(true);
            fStatus->setText(error.isEmpty() ? tr("Exported %1. Review warnings and source hashes before deployment.").arg(path) : error);
        });
        const XcfgDocument snapshot = fDocument;
        watcher->setFuture(QtConcurrent::run([snapshot, path]() {
            QString error; HardwareCompiler::Save(snapshot, path, &error); return error;
        }));
    });
    connect(fApply, &QPushButton::clicked, this, [this]() {
        if (fReadOnly || !fTree->currentItem()) return;
        const int id = fTree->currentItem()->data(0, Qt::UserRole).toInt();
        SaveValueAsync(id,fValue->text());
    });
    SelectionChanged();
}
bool ConfigWindow::Open(const QString &path, QString *error)
{
    ++fRevision; fSnapshotReview->clear(); fSnapshotReview->hide();
    ClearOverride();
    if (!fDocument.Load(path, error)) return false;
    fHistory.Clear();UpdateHistoryButtons();
    fDirectory = QFileInfo(path).absolutePath(); Rebuild();
    fStatus->setText(tr("Loaded %1 source files, %2 declarations. %3").arg(fDocument.Sources().size()).arg(fDocument.Nodes().size())
        .arg(fReadOnly ? tr("Read-only while connected.") : tr("Editing a '*' declaration can affect many inherited instances.")));
    return true;
}
