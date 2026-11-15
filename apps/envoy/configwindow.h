#ifndef GETDAQ_CONFIGWINDOW_H
#define GETDAQ_CONFIGWINDOW_H
#include "xcfgdocument.h"
#include "xcfgedithistory.h"
#include <QDialog>
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QLineEdit;
class QPushButton;
class QPlainTextEdit;
class QTabWidget;
class QJsonObject;
class ConfigWindow : public QDialog {
    Q_OBJECT
public:
    explicit ConfigWindow(const QString &directory, bool readOnly, QWidget *parent = nullptr);
    bool Open(const QString &path, QString *error);
    bool ExportEffectiveSnapshot(const QString &path,QString *error);
    bool CompareEffectiveSnapshot(const QString &path,QString *error);
    bool PublishSystemConfiguration(const QString &profile,const QString &directory,QString *error);
private:
    void Rebuild();
    void AddNode(QTreeWidgetItem *parent, int index);
    void SelectionChanged();
    void OpenStructureEditor();
    void OpenXmlExport();
    void OpenWorkspaceCopy();
    void OpenHistoryReview(bool redo);
    void OpenValueEditor();
    bool SaveValueAsync(int id, const QString &value);
    void UpdateHistoryButtons();
    XcfgDocument fDocument;
    XcfgEditHistory fHistory;
    QString fDirectory;
    QTreeWidget *fTree;
    QLabel *fStatus;
    QLineEdit *fValue;
    QPushButton *fApply;
    QPushButton *fEditValue;
    QPushButton *fOpenButton;
    QLabel *fSelectionInfo;
    QString fSelectedValue;
    QPushButton *fStructureButton;
    QPushButton *fUndo;
    QPushButton *fRedo;
    bool fReadOnly;
    bool fHaveOverride = false;
    XcfgDocument::OverridePreview fOverride;
    QPlainTextEdit *fOverrideText;
    QPushButton *fOverrideApply;
    QPlainTextEdit *fSnapshotReview;
    int fRevision=0;
    bool fValueEditPending=false;
    void ClearOverride();
    void ShowSnapshotReport(const QJsonObject &report);
    void StartSnapshotTask(const QString &path,bool compare,QPushButton *exportButton,QPushButton *compareButton);
};
#endif
