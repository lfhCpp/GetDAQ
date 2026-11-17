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
    explicit IPWindow(const RunConfig &config, QWidget *parent = nullptr, const QVector<NodeStatus> &status = {});
    RunConfig GetConfig() const { return fConfig; }
private slots:
    void ApplySlot();
    void AddNodeSlot();
    void RemoveNodeSlot();
private:
    void AddRow(const NodeConfig &node);
    RunConfig fConfig;
    QTableWidget *fNodes;
    QLabel *fLegacyTopologyNote;
    QLineEdit *fConfigDir;
    QLineEdit *fOutputDir;
    QSpinBox *fPoll;
    QSpinBox *fTimeout;
    QSpinBox *fCatalogTimeout;
    QSpinBox *fStepTimeout;
    QSpinBox *fSettle;
};
#endif
