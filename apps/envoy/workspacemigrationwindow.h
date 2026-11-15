#ifndef GETDAQ_WORKSPACEMIGRATIONWINDOW_H
#define GETDAQ_WORKSPACEMIGRATIONWINDOW_H
#include "xcfgsourcemigration.h"
#include <QDialog>
namespace Ui {class WorkspaceMigrationWindow;}
class WorkspaceMigrationWindow : public QDialog {
    Q_OBJECT
public:
    explicit WorkspaceMigrationWindow(XcfgWorkspace *workspace,bool readOnly,QWidget *parent=nullptr);
    ~WorkspaceMigrationWindow() override;
    XcfgSourceMigration::Result Result() const {return fResult;}
private slots:
    void ChooseDestination();
    void Invalidate();
    void Preview();
    void Apply();
    void ShowSource();
private:
    Ui::WorkspaceMigrationWindow *ui;
    XcfgWorkspace *fWorkspace;
    bool fReadOnly,fHavePreview=false;
    quint64 fReviewGeneration=0;
    XcfgSourceMigration::Preview fPreview;
    XcfgSourceMigration::Result fResult;
};
#endif
