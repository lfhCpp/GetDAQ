#ifndef GETDAQ_HOST_ROLE_WINDOW_H
#define GETDAQ_HOST_ROLE_WINDOW_H
#include "runconfig.h"
#include <QDialog>
#include <QJsonObject>
#include <functional>
#include <memory>
#include <atomic>
class QComboBox;class QStackedWidget;class QLabel;class QLineEdit;class QSpinBox;class QTableWidget;class QPushButton;
class UnifiedEccWindow;
class HostRoleWindow:public QDialog {
    Q_OBJECT
public:
    explicit HostRoleWindow(const RunConfig &base,QWidget *parent=nullptr);
    RunConfig Configuration() const{return fSelected;}
    static QString SavedRole();
    static QString ButtonText();
    static void ResumeSlaveAgent(QObject *owner,std::function<void(QString)> report);
protected:
    void reject() override;
private:
    RunConfig fBase,fSelected;
    QComboBox *fRole;
    QStackedWidget *fPages;
    UnifiedEccWindow *fMaster=nullptr;
    QLabel *fStatus;
    QLineEdit *fAddress,*fData,*fCache,*fInvitation;
    QSpinBox *fPort;
    QTableWidget *fInstances;
    QPushButton *fStart,*fCopy;
    QString fRoot;
    QJsonObject fNode;
    bool fBusy=false;
    bool fObserving=false;
    bool fCloseAfterWait=false;
    std::shared_ptr<std::atomic_bool> fCanceled;
    void SelectRole(int index);
    void StartSlave();
    void RenewPairing();
private slots:
    void ObserveSlave();
    void ShowNode(const QJsonObject &node);
};
#endif
