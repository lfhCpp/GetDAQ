#include "configwindow.h"
#include "xcfgstructureeditor.h"
#include <QComboBox>
#include <QCryptographicHash>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <memory>

void ConfigWindow::OpenStructureEditor()
{
    if (fReadOnly) return;
    const int node=fTree->currentItem() ? fTree->currentItem()->data(0,Qt::UserRole).toInt() : -1;
    if (node<0 || node>=fDocument.Nodes().size()) {fStatus->setText(tr("Select an XML declaration in the tree first."));return;}
    const auto selected=fDocument.Nodes()[node];const int revision=fRevision;
    auto dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setObjectName("ConfigStructureDialog");
    dialog->setWindowTitle(tr("Review configuration structure change"));dialog->resize(1000,700);
    auto layout=new QVBoxLayout(dialog);
    auto description=new QLabel(tr("Selected source: %1\nDeclaration: %2\nChanges remain local. An included/default declaration may affect several instances. Review every listed change before applying.")
        .arg(selected.file,selected.path),dialog);description->setWordWrap(true);layout->addWidget(description);
    auto form=new QFormLayout;
    auto operation=new QComboBox(dialog);operation->setObjectName("ConfigStructureOperation");
    operation->addItems({tr("Add child object"),tr("Add child scalar"),tr("Delete selected declaration"),tr("Change selected ID"),tr("Materialize current effective children"),
        tr("Change selected element name"),tr("Clone selected declaration"),tr("Move before sibling"),tr("Move after sibling"),tr("Edit scalar type/unit/range"),
        tr("Replace children with local include"),tr("Change/remove local include path"),tr("Import local configuration children"),tr("Shallow override inherited object by query path")});
    auto sibling=new QComboBox(dialog);sibling->setObjectName("ConfigStructureSibling");sibling->addItem(tr("Choose another sibling"),-1);
    for (const auto &parent:fDocument.Nodes()) if (parent.children.contains(node)) for (int child:parent.children) if (child!=node) {
        const auto other=fDocument.Nodes()[child];sibling->addItem(other.path,child);
    }
    auto name=new QLineEdit(dialog);name->setObjectName("ConfigStructureName");
    auto id=new QLineEdit(dialog);id->setObjectName("ConfigStructureId");
    auto value=new QLineEdit(dialog);value->setObjectName("ConfigStructureValue");
    auto type=new QComboBox(dialog);type->setObjectName("ConfigStructureType");type->addItems({"int","hex","real","bool","string"});
    type->addItem(tr("Auto type (no explicit declaration)"));
    auto unit=new QLineEdit(dialog);unit->setObjectName("ConfigStructureUnit");
    auto range=new QLineEdit(dialog);range->setObjectName("ConfigStructureRange");
    auto includePath=new QLineEdit(dialog);includePath->setObjectName("ConfigStructureIncludePath");
    auto objectPath=new QLineEdit(dialog);objectPath->setObjectName("ConfigStructureObjectPath");
    const auto lookup=findChild<QLineEdit *>("ConfigLookupPath");objectPath->setText(lookup && !lookup->text().trimmed().isEmpty() ? lookup->text().trimmed() : selected.path);
    objectPath->setPlaceholderText(tr("Inherited object path; '*' also copies required same-name siblings"));
    includePath->setPlaceholderText(tr("Local file path, relative to the selected source file"));
    form->addRow(tr("Operation"),operation);form->addRow(tr("New element name"),name);form->addRow(tr("ID (empty for no ID)"),id);form->addRow(tr("Reference sibling"),sibling);
    form->addRow(tr("Scalar type"),type);form->addRow(tr("Scalar value"),value);form->addRow(tr("Shared unit"),unit);form->addRow(tr("Shared range"),range);
    form->addRow(tr("Include/import file"),includePath);form->addRow(tr("Object override query"),objectPath);layout->addLayout(form);
    auto tabs=new QTabWidget(dialog);
    auto changeTable=new QTreeWidget(dialog);changeTable->setObjectName("ConfigStructureChangeTable");
    changeTable->setHeaderLabels({tr("Path"),tr("Change"),tr("Before"),tr("After")});changeTable->setColumnWidth(0,310);
    auto original=new QPlainTextEdit(dialog);original->setObjectName("ConfigStructureOriginal");original->setReadOnly(true);
    auto updated=new QPlainTextEdit(dialog);updated->setObjectName("ConfigStructureUpdated");updated->setReadOnly(true);
    auto changes=new QPlainTextEdit(dialog);changes->setObjectName("ConfigStructureChanges");changes->setReadOnly(true);
    auto dependencies=new QTreeWidget(dialog);dependencies->setObjectName("ConfigStructureDependencies");
    dependencies->setHeaderLabels({tr("Read-only source file"),tr("Bytes"),tr("SHA-256")});dependencies->setColumnWidth(0,460);
    auto created=new QTreeWidget(dialog);created->setObjectName("ConfigStructureCreatedObjects");created->setHeaderLabels({tr("Created shallow declaration path")});
    auto localized=new QTreeWidget(dialog);localized->setObjectName("ConfigStructureLocalizedObjects");localized->setHeaderLabels({tr("Every concrete projection now using a local object declaration")});
    tabs->addTab(changeTable,tr("Change summary"));tabs->addTab(changes,tr("Full review record"));tabs->addTab(dependencies,tr("Input dependencies"));
    tabs->addTab(original,tr("Original XML"));tabs->addTab(updated,tr("Proposed XML"));layout->addWidget(tabs,1);
    tabs->addTab(created,tr("Created object layers"));
    tabs->addTab(localized,tr("Affected concrete projections"));
    auto status=new QLabel(dialog);status->setObjectName("ConfigStructureStatus");status->setTextFormat(Qt::PlainText);status->setWordWrap(true);layout->addWidget(status);
    auto row=new QHBoxLayout;
    auto preview=new QPushButton(tr("Preview change"),dialog);preview->setObjectName("ConfigStructurePreview");
    auto apply=new QPushButton(tr("Apply reviewed change"),dialog);apply->setObjectName("ConfigStructureApply");apply->setEnabled(false);
    auto close=new QPushButton(tr("Close"),dialog);row->addWidget(preview);row->addWidget(apply);row->addWidget(close);layout->addLayout(row);
    auto proposed=std::make_shared<XcfgStructureEditor::Preview>();
    const auto clear=[apply,original,updated,changes,changeTable,dependencies,created,localized] {apply->setEnabled(false);original->clear();updated->clear();changes->clear();changeTable->clear();dependencies->clear();created->clear();localized->clear();};
    const auto fields=[operation,name,id,type,value,unit,range,sibling,includePath,objectPath,clear,selected] {
        clear();const int action=operation->currentIndex();const bool scalar=action==XcfgStructureEditor::AddScalar;
        const bool properties=action==XcfgStructureEditor::ChangeScalarProperties;
        name->setEnabled(action==XcfgStructureEditor::AddObject || scalar || action==XcfgStructureEditor::ChangeName || action==XcfgStructureEditor::CloneDeclaration);
        id->setEnabled(action==XcfgStructureEditor::AddObject || scalar || action==XcfgStructureEditor::ChangeId || action==XcfgStructureEditor::CloneDeclaration);
        sibling->setEnabled(action==XcfgStructureEditor::MoveBefore || action==XcfgStructureEditor::MoveAfter);
        type->setEnabled(scalar || properties);value->setEnabled(scalar);unit->setEnabled(scalar || properties);range->setEnabled(scalar || properties);
        includePath->setEnabled(action==XcfgStructureEditor::ReplaceWithInclude || action==XcfgStructureEditor::ChangeInclude || action==XcfgStructureEditor::ImportConfiguration);
        objectPath->setEnabled(action==XcfgStructureEditor::OverrideInheritedObject);
        if(action==XcfgStructureEditor::ChangeInclude)includePath->setText(selected.attributes.value("include"));
        if (properties) {
            const int index=type->findText(selected.attributes.value("type"));type->setCurrentIndex(index<0 ? 5 : index);
            unit->setText(selected.attributes.value("unit"));range->setText(selected.attributes.value("range"));
        }
    };
    for (auto field:{name,id,value,unit,range,includePath,objectPath}) connect(field,&QLineEdit::textChanged,dialog,clear);
    connect(type,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,clear);
    connect(sibling,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,clear);
    connect(operation,QOverload<int>::of(&QComboBox::currentIndexChanged),dialog,fields);
    connect(close,&QPushButton::clicked,dialog,&QDialog::close);
    connect(preview,&QPushButton::clicked,dialog,[this,revision,node,operation,name,id,type,value,unit,range,sibling,includePath,objectPath,proposed,apply,original,updated,changes,changeTable,dependencies,created,localized,status,clear] {
        clear();if (fReadOnly || revision!=fRevision) {status->setText(tr("Configuration changed; close this dialog and select the declaration again."));return;}
        XcfgStructureEditor::Request request;request.operation=static_cast<XcfgStructureEditor::Operation>(operation->currentIndex());request.node=node;
        request.name=name->text().trimmed();request.id=id->text();request.type=type->currentIndex()==5 ? QString() : type->currentText();request.value=value->text();request.unit=unit->text();request.range=range->text();
        request.sibling=sibling->currentData().toInt();
        request.includePath=includePath->text();
        request.path=objectPath->text().trimmed();
        QString error;if (!XcfgStructureEditor::CreatePreview(fDocument,request,proposed.get(),&error)) {status->setText(error);return;}
        original->setPlainText(QString::fromUtf8(proposed->originalBytes));updated->setPlainText(QString::fromUtf8(proposed->updatedBytes));
        changes->setPlainText(QString::fromUtf8(QJsonDocument(proposed->report).toJson(QJsonDocument::Indented)));
        for(const auto &path:proposed->createdPaths)new QTreeWidgetItem(created,{path});
        for(const auto &path:proposed->localizedPaths)new QTreeWidgetItem(localized,{path});
        for(auto it=proposed->dependencyBytes.constBegin();it!=proposed->dependencyBytes.constEnd();++it)
            new QTreeWidgetItem(dependencies,{it.key(),QString::number(it.value().size()),QString::fromLatin1(QCryptographicHash::hash(it.value(),QCryptographicHash::Sha256).toHex())});
        const auto describe=[](const QJsonObject &scalar) -> QString {
            if (scalar.isEmpty()) return QString();
            const auto origin=scalar.value("value_source").toObject();
            return scalar.value("value").toString()+" ["+scalar.value("type").toString()+"] "+scalar.value("unit").toString()+" "+scalar.value("range").toString()+
                "\n"+origin.value("file").toString()+":"+QString::number(origin.value("line").toInt());
        };
        for (const auto &item:proposed->report.value("scalar_changes").toArray()) {
            const auto entry=item.toObject();new QTreeWidgetItem(changeTable,{entry.value("path").toString(),entry.value("change").toString(),describe(entry.value("before").toObject()),describe(entry.value("after").toObject())});
        }
        for (const auto &item:proposed->report.value("object_changes").toArray()) {
            const auto entry=item.toObject();new QTreeWidgetItem(changeTable,{entry.value("path").toString(),tr("Object: %1").arg(entry.value("change").toString())});
        }
        status->setText(tr("Preview only; no file written.\nEffective parameters changed: %1; scalar changes: %2; object changes: %3.\nDestination: %4; shallow objects created: %5. Saving will recheck all loaded sources. Hardware loading is not verified.")
            .arg(proposed->report.value("effective_changed").toBool() ? tr("yes") : tr("no"))
            .arg(proposed->report.value("scalar_changes").toArray().size()).arg(proposed->report.value("object_changes").toArray().size())
            .arg(proposed->destinationFile).arg(proposed->createdPaths.size()));
        apply->setEnabled(true);
    });
    connect(apply,&QPushButton::clicked,dialog,[this,revision,proposed,apply,status,dialog] {
        if (!apply->isEnabled() || fReadOnly) return;
        apply->setEnabled(false);
        if (revision!=fRevision) {status->setText(tr("Configuration changed; create a new preview."));return;}
        QString error;
        if (!fHistory.ApplyStructure(&fDocument,*proposed,&error)) {UpdateHistoryButtons();if(fDocument.Nodes().isEmpty()) Rebuild();status->setText(error);return;}
        const auto destination=proposed->destinationFile;Rebuild();
        fStatus->setText(tr("Saved reviewed structure change to %1. Other source files were preserved. No hardware command was sent.").arg(destination));dialog->close();
    });
    fields();dialog->show();
}
