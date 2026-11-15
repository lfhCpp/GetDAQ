#ifndef GETDAQ_AUTORUN_WIDGET_H
#define GETDAQ_AUTORUN_WIDGET_H
#include "autorun.h"
#include <QWidget>
class QCheckBox;class QComboBox;class QDoubleSpinBox;class QSpinBox;class QLabel;class QPushButton;
class AutoRunWidget : public QWidget {
    Q_OBJECT
public:
    explicit AutoRunWidget(QWidget *parent=nullptr);
    AutoRunSettings Settings() const;
    void SetSettings(const AutoRunSettings &settings);
    void SetStatus(const QString &text,bool armed);
    void SetLocked(bool locked);
    void SetEnabledState(bool enabled);
signals:
    void ApplyRequested(AutoRunSettings settings);
    void EnableRequested(bool enabled);
private:
    QCheckBox *fEnabled,*fTimeEnabled,*fSizeEnabled;
    bool fUpdating=false,fPending=false,fLocked=false;
    QComboBox *fAction,*fTimeUnit,*fSizeUnit;
    QDoubleSpinBox *fTime,*fSize;
    QSpinBox *fRuns;
    QLabel *fStatus;
    QPushButton *fApply;
    void UpdateInputs();
};
#endif
