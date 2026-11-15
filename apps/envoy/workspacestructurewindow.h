#ifndef GETDAQ_WORKSPACESTRUCTUREWINDOW_H
#define GETDAQ_WORKSPACESTRUCTUREWINDOW_H
#include "xcfgworkspace.h"
#include <QDialog>
namespace Ui {class WorkspaceStructureWindow;}
class WorkspaceStructureWindow : public QDialog {
    Q_OBJECT
public:
    explicit WorkspaceStructureWindow(XcfgWorkspace *workspace,QWidget *parent=nullptr);
    ~WorkspaceStructureWindow() override;
private slots:
    void LoadRoot();
    void LoadTargetRoot();
    void SelectDeclaration();
    void RefreshFields();
    void Invalidate();
    void Preview();
    void Stage();
private:
    enum { CrossFileMoveOperation=XcfgStructureEditor::OverrideInheritedObject+1 };
    Ui::WorkspaceStructureWindow *ui;
    XcfgWorkspace *fWorkspace;
    XcfgDocument fDocument;
    XcfgDocument fTargetDocument;
    XcfgWorkspace::StructurePreview fPreview;
    XcfgWorkspace::MovePreview fMovePreview;
    bool fHavePreview=false;
};
#endif
