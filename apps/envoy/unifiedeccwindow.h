#ifndef GETDAQ_UNIFIED_ECC_WINDOW_H
#define GETDAQ_UNIFIED_ECC_WINDOW_H
#include "existingeccconnection.h"
#include <QDialog>
#include <memory>
#include <atomic>
class QComboBox;class QLineEdit;class QSpinBox;class QLabel;class QPushButton;class QTabWidget;class QCheckBox;class DistributedDeploymentWindow;
class UnifiedEccWindow:public QDialog {
    Q_OBJECT
public:
    explicit UnifiedEccWindow(const RunConfig &base,QWidget *parent=nullptr);
    RunConfig Configuration() const{return fSelected;}
    bool Busy() const;
    void RequestClose();
protected:
    void reject() override;
private:
    RunConfig fBase,fConnectionBase,fSelected;
    ExistingEccResult fChecked;
    QLineEdit *fName,*fAddress,*fProfile,*fReceiver,*fOutput;
    QSpinBox *fPort,*fDataPort;
    QLabel *fStatus;
    QPushButton *fCheck,*fUse,*fCancel;
    QTabWidget *fTabs;
    QCheckBox *fAdvanced;
    DistributedDeploymentWindow *fManaged;
    bool fBusy=false,fCloseAfterWait=false;
    QByteArray fCheckedInputs;
    std::shared_ptr<std::atomic_bool> fCanceled;
    QByteArray Inputs() const;
    void Check();
    void Use();
};
#endif
