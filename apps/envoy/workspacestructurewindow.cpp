#include "workspacestructurewindow.h"
#include "ui_workspacestructurewindow.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSignalBlocker>
WorkspaceStructureWindow::WorkspaceStructureWindow(XcfgWorkspace *workspace,QWidget *parent)
    : QDialog(parent),ui(new Ui::WorkspaceStructureWindow),fWorkspace(workspace) {
    ui->setupUi(this);
    ui->root->addItems(workspace->Roots());
    ui->targetRoot->addItems(workspace->Roots());
    ui->operation->addItems({tr("Add child object"),tr("Add child scalar"),tr("Delete declaration"),tr("Change ID"),tr("Materialize effective children"),tr("Change element name"),tr("Clone declaration"),tr("Move before sibling"),tr("Move after sibling"),tr("Edit scalar type/unit/range"),tr("Replace children with include"),tr("Change/remove include"),tr("Import configuration children"),tr("Shallow override inherited object")});
    ui->operation->addItem(tr("Move declaration to another source file"));
    ui->type->addItems({"int","hex","real","bool","string",tr("Auto type")});
    connect(ui->root,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::LoadRoot);
    connect(ui->declaration,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::SelectDeclaration);
    connect(ui->targetRoot,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::LoadTargetRoot);
    connect(ui->targetParent,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::Invalidate);
    connect(ui->operation,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::RefreshFields);
    for(auto field:{ui->name,ui->id,ui->value,ui->unit,ui->range,ui->includePath,ui->objectPath})connect(field,&QLineEdit::textChanged,this,&WorkspaceStructureWindow::Invalidate);
    for(auto field:{ui->type,ui->sibling})connect(field,QOverload<int>::of(&QComboBox::currentIndexChanged),this,&WorkspaceStructureWindow::Invalidate);
    connect(ui->preview,&QPushButton::clicked,this,&WorkspaceStructureWindow::Preview);
    connect(ui->stage,&QPushButton::clicked,this,&WorkspaceStructureWindow::Stage);
    connect(ui->close,&QPushButton::clicked,this,&QDialog::reject);LoadRoot();LoadTargetRoot();
}
WorkspaceStructureWindow::~WorkspaceStructureWindow(){delete ui;}
void WorkspaceStructureWindow::Invalidate(){fHavePreview=false;ui->stage->setEnabled(false);ui->review->clear();ui->before->clear();ui->after->clear();}
void WorkspaceStructureWindow::LoadRoot(){
    Invalidate();QString error;const QSignalBlocker blocked(ui->declaration);ui->declaration->clear();
    if(!fWorkspace->Document(ui->root->currentText(),&fDocument,&error)){ui->preview->setEnabled(false);ui->status->setText(error);return;}
    ui->preview->setEnabled(true);for(int i=0;i<fDocument.Nodes().size();++i){const auto &n=fDocument.Nodes()[i];ui->declaration->addItem(n.path+" — "+QFileInfo(n.file).fileName(),i);}SelectDeclaration();
}
void WorkspaceStructureWindow::SelectDeclaration(){
    Invalidate();const auto index=ui->declaration->currentData().toInt();if(index<0 || index>=fDocument.Nodes().size())return;
    const auto &node=fDocument.Nodes()[index];ui->source->setText(node.file+"\n"+node.path);
    {const QSignalBlocker blocked(ui->sibling);ui->sibling->clear();ui->sibling->addItem(tr("Select a sibling"),-1);
     for(const auto &parent:fDocument.Nodes())if(parent.children.contains(index))for(int child:parent.children)if(child!=index)ui->sibling->addItem(fDocument.Nodes()[child].path,child);}
    ui->objectPath->setText(node.path);RefreshFields();
}
void WorkspaceStructureWindow::LoadTargetRoot(){
    Invalidate();QString error;const QSignalBlocker blocked(ui->targetParent);ui->targetParent->clear();
    if(!fWorkspace->Document(ui->targetRoot->currentText(),&fTargetDocument,&error)){ui->status->setText(error);return;}
    for(int i=0;i<fTargetDocument.Nodes().size();++i){const auto &n=fTargetDocument.Nodes()[i];if(!n.scalar)ui->targetParent->addItem(n.path+" — "+QFileInfo(n.file).fileName(),i);}
}
void WorkspaceStructureWindow::RefreshFields(){
    Invalidate();const int op=ui->operation->currentIndex();const bool scalar=op==XcfgStructureEditor::AddScalar,properties=op==XcfgStructureEditor::ChangeScalarProperties;
    ui->name->setEnabled(op==XcfgStructureEditor::AddObject || scalar || op==XcfgStructureEditor::ChangeName || op==XcfgStructureEditor::CloneDeclaration);
    ui->id->setEnabled(op==XcfgStructureEditor::AddObject || scalar || op==XcfgStructureEditor::ChangeId || op==XcfgStructureEditor::CloneDeclaration);
    ui->sibling->setEnabled(op==XcfgStructureEditor::MoveBefore || op==XcfgStructureEditor::MoveAfter);
    ui->type->setEnabled(scalar || properties);ui->value->setEnabled(scalar);ui->unit->setEnabled(scalar || properties);ui->range->setEnabled(scalar || properties);
    ui->includePath->setEnabled(op==XcfgStructureEditor::ReplaceWithInclude || op==XcfgStructureEditor::ChangeInclude || op==XcfgStructureEditor::ImportConfiguration);ui->objectPath->setEnabled(op==XcfgStructureEditor::OverrideInheritedObject);
    ui->targetRoot->setEnabled(op==CrossFileMoveOperation);ui->targetParent->setEnabled(op==CrossFileMoveOperation);
    const int index=ui->declaration->currentData().toInt();if(index<0 || index>=fDocument.Nodes().size())return;const auto &node=fDocument.Nodes()[index];
    if(properties){const auto type=ui->type->findText(node.attributes.value("type"));ui->type->setCurrentIndex(type<0 ? 5 : type);ui->unit->setText(node.attributes.value("unit"));ui->range->setText(node.attributes.value("range"));}
    if(op==XcfgStructureEditor::ChangeInclude)ui->includePath->setText(node.attributes.value("include"));
}
void WorkspaceStructureWindow::Preview(){
    if(ui->operation->currentIndex()==CrossFileMoveOperation) {
        Invalidate();XcfgWorkspace::MoveRequest request;request.sourceRoot=ui->root->currentText();request.sourceNode=ui->declaration->currentData().toInt();request.targetRoot=ui->targetRoot->currentText();request.targetParent=ui->targetParent->currentData().toInt();
        QString error;if(!ui->targetParent->currentData().isValid() || !fWorkspace->PreviewMove(request,&fMovePreview,&error)){ui->status->setText(error.isEmpty() ? tr("Select a destination object") : error);return;}
        QString before,after;for(auto it=fMovePreview.before.constBegin();it!=fMovePreview.before.constEnd();++it)before+=it.key()+"\n"+QString::fromUtf8(it.value())+"\n\n";for(auto it=fMovePreview.after.constBegin();it!=fMovePreview.after.constEnd();++it)after+=it.key()+"\n"+QString::fromUtf8(it.value())+"\n\n";
        ui->before->setPlainText(before);ui->after->setPlainText(after);QJsonObject report=fMovePreview.report,roots;for(auto it=fMovePreview.workspace.reports.constBegin();it!=fMovePreview.workspace.reports.constEnd();++it)roots[it.key()]=it.value();report["all_root_reviews"]=roots;
        ui->review->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson()));fHavePreview=true;ui->stage->setEnabled(true);ui->status->setText(tr("Review both changed files, rewritten include paths and every root. The move stages as one Undo/Redo group; Save All remains a sequence of file saves, not a transaction."));return;
    }
    Invalidate();XcfgStructureEditor::Request r;r.operation=static_cast<XcfgStructureEditor::Operation>(ui->operation->currentIndex());r.node=ui->declaration->currentData().toInt();r.sibling=ui->sibling->currentData().toInt();r.name=ui->name->text().trimmed();r.id=ui->id->text();r.type=ui->type->currentIndex()==5 ? QString() : ui->type->currentText();r.value=ui->value->text();r.unit=ui->unit->text();r.range=ui->range->text();r.includePath=ui->includePath->text();r.path=ui->objectPath->text().trimmed();
    QString error;if(!fWorkspace->PreviewStructure(ui->root->currentText(),r,&fPreview,&error)){ui->status->setText(error);return;}
    ui->before->setPlainText(QString::fromUtf8(fPreview.edit.originalBytes));ui->after->setPlainText(QString::fromUtf8(fPreview.edit.updatedBytes));
    QJsonObject report;report["selected_structure_change"]=fPreview.edit.report;QJsonObject roots;for(auto it=fPreview.workspace.reports.constBegin();it!=fPreview.workspace.reports.constEnd();++it)roots[it.key()]=it.value();report["all_root_reviews"]=roots;
    QJsonArray files;for(auto it=fPreview.workspace.writes.constBegin();it!=fPreview.workspace.writes.constEnd();++it)files.append(it.key());report["all_pending_files"]=files;report["hardware_loaded_verified"]=false;report["disk_written"]=false;
    ui->review->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson()));fHavePreview=true;ui->stage->setEnabled(true);ui->status->setText(tr("Inspect the structural change and every root review. Staging joins the same workspace history; Save All is still required to write disk."));
}
void WorkspaceStructureWindow::Stage(){
    if(!fHavePreview)return;
    QString error;const bool okay=ui->operation->currentIndex()==CrossFileMoveOperation ? fWorkspace->StageMove(fMovePreview,&error) : fWorkspace->StageStructure(fPreview,&error);if(!okay){Invalidate();ui->status->setText(error);return;}accept();
}
