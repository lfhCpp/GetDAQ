#ifndef IPWINDOW_H
#define IPWINDOW_H
#include "runconfig.h"
#include <QDialog>
class QTableWidget;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;

class IPWindow : public QDialog
{
    Q_OBJECT
public:
    explicit IPWindow(const RunConfig &config, QWidget *parent = nullptr);
    RunConfig GetConfig() const { return fConfig; }
private slots:
    void ApplySlot();
    void AddNodeSlot();
    void RemoveNodeSlot();
private:
    void AddRow(const NodeConfig &node);
    RunConfig fConfig;
    QTableWidget *fNodes;
    QLabel *fLegacyTopologyNote,*fNativeTopologyNote;
    QLineEdit *fConfigDir;
    QLineEdit *fOutputDir;
    QSpinBox *fPoll;
    QSpinBox *fTimeout;
    QSpinBox *fCatalogTimeout;
    QSpinBox *fStepTimeout;
    QSpinBox *fSettle;
    QCheckBox *fNative,*fNativeCatalog,*fNativeAlarms,*fNativeReceivers,*fNativeSources,*fNativeReceiverFailure;
    QLineEdit *fNativeHost, *fNativeCredential, *fNativeService, *fNativeClient, *fNativeEpoch, *fNativeSnapshot, *fNativeInput, *fNativeRecords;
    QSpinBox *fNativePort, *fNativeTimeout, *fNativeStatusAge;
};
#endif
