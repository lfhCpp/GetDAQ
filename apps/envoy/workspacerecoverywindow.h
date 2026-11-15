#ifndef GETDAQ_WORKSPACERECOVERYWINDOW_H
#define GETDAQ_WORKSPACERECOVERYWINDOW_H
#include "xcfgsavejournal.h"
#include "xcfgsourcemigration.h"
#include <QDialog>
namespace Ui {class WorkspaceRecoveryWindow;}
class WorkspaceRecoveryWindow : public QDialog {
    Q_OBJECT
public:
    explicit WorkspaceRecoveryWindow(const QString &directory,bool readOnly,QWidget *parent=nullptr);
    ~WorkspaceRecoveryWindow() override;
private slots:
    void Choose();
    void Invalidate();
    void Preview();
    void Apply();
    void ShowSource();
private:
    Ui::WorkspaceRecoveryWindow *ui;
    XcfgSaveJournal::Recovery fPreview;
    XcfgSourceMigration::Recovery fMigration;
    bool fMigrationRecord=false;
    bool fReadOnly,fHavePreview=false;
    quint64 fReviewGeneration=0;
};
#endif
