#ifndef GETDAQ_CONFIGURATIONREPOSITORYWINDOW_H
#define GETDAQ_CONFIGURATIONREPOSITORYWINDOW_H
#include "configurationrepositoryclient.h"
#include <QDialog>
class QLineEdit;class QSpinBox;class QPushButton;class QPlainTextEdit;class QComboBox;
class ConfigurationRepositoryWindow : public QDialog {
    Q_OBJECT
public:
    explicit ConfigurationRepositoryWindow(QWidget *parent=nullptr);
private:
    ConfigurationRepositoryClient *fClient=nullptr;
    QLineEdit *fAddress,*fCredential,*fId;
    QSpinBox *fPort;
    QPushButton *fIdentity,*fList,*fUpload,*fResume,*fVerify,*fBrowse,*fExport;
    QPlainTextEdit *fResult;
    QComboBox *fVersions;
    bool fBusy=false;
    void MakeClient();
    void Busy(bool busy);
    void ClearClient();
};
#endif
