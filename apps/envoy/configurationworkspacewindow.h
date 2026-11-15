#ifndef GETDAQ_CONFIGURATIONWORKSPACEWINDOW_H
#define GETDAQ_CONFIGURATIONWORKSPACEWINDOW_H
#include "xcfgworkspace.h"
#include <QDialog>
#include <QSet>
namespace Ui { class ConfigurationWorkspaceWindow; }
class QPlainTextEdit;

class ConfigurationWorkspaceWindow : public QDialog {
    Q_OBJECT
public:
    explicit ConfigurationWorkspaceWindow(const QString &directory, bool readOnly, QWidget *parent = nullptr);
    ~ConfigurationWorkspaceWindow();
    bool Open(const QString &path, QString *error);
protected:
    void closeEvent(QCloseEvent *event) override;
private slots:
    void ChooseRoot();
    void ReviewAll();
    void SaveAll();
    void MigrateSource();
    void InvalidateReview();
    void Undo();
    void Redo();
    void EditStructure();
    void InspectRecovery();
private:
    Ui::ConfigurationWorkspaceWindow *ui;
    XcfgWorkspace fWorkspace;
    XcfgWorkspace::Preview fPreview;
    QMap<QString,QPlainTextEdit *> fEditors;
    QSet<QString> fPendingFiles;
    QString fDirectory;
    bool fReadOnly, fHaveReview = false;
    QString fRecoveryDirectory;
    void AddSourceTabs();
    bool HasPendingText() const;
    void RefreshHistory();
    void ApplyHistory(bool redo);
    void SyncEditors();
};
#endif
