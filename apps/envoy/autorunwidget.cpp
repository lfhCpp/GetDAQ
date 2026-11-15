#include "autorunwidget.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QScopedValueRollback>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSignalBlocker>
#include <QtMath>

AutoRunWidget::AutoRunWidget(QWidget *parent):QWidget(parent) {
    setObjectName("AutoRunControls");setMinimumWidth(310);setMaximumWidth(360);
    auto *layout=new QVBoxLayout(this);layout->setContentsMargins(8,0,0,0);layout->setAlignment(Qt::AlignVCenter);layout->setSpacing(2);
    auto *header=new QHBoxLayout;auto *title=new QLabel(tr("Automatic Run"),this);title->setObjectName("AutoRunTitle");
    fEnabled=new QCheckBox(tr("Enable"),this);fEnabled->setObjectName("AutoRunEnabled");
    header->addWidget(title);header->addStretch();header->addWidget(fEnabled);layout->addLayout(header);
    fTimeEnabled=new QCheckBox(tr("Time"),this);fTimeEnabled->setObjectName("AutoRunTimeEnabled");fTimeEnabled->setChecked(true);
    fSizeEnabled=new QCheckBox(tr("File size"),this);fSizeEnabled->setObjectName("AutoRunSizeEnabled");
    fTimeEnabled->setToolTip(tr("Select one condition, or select both: the first threshold reached triggers the action."));
    fSizeEnabled->setToolTip(fTimeEnabled->toolTip());
    auto *timePage=new QWidget(this);auto *timeLayout=new QHBoxLayout(timePage);timeLayout->setContentsMargins(0,0,0,0);timeLayout->setSpacing(4);
    fTime=new QDoubleSpinBox(timePage);fTime->setObjectName("AutoRunTime");fTime->setDecimals(1);fTime->setRange(0.1,31536000);fTime->setValue(10);
    fTimeUnit=new QComboBox(timePage);fTimeUnit->setObjectName("AutoRunTimeUnit");fTimeUnit->addItem(tr("s"),1000LL);fTimeUnit->addItem(tr("min"),60000LL);fTimeUnit->addItem(tr("h"),3600000LL);fTimeUnit->setCurrentIndex(1);
    timeLayout->addWidget(fTimeEnabled);timeLayout->addWidget(fTime,1);timeLayout->addWidget(fTimeUnit);layout->addWidget(timePage);
    auto *sizePage=new QWidget(this);auto *sizeLayout=new QHBoxLayout(sizePage);sizeLayout->setContentsMargins(0,0,0,0);sizeLayout->setSpacing(4);
    fSize=new QDoubleSpinBox(sizePage);fSize->setObjectName("AutoRunSize");fSize->setDecimals(3);fSize->setRange(0.001,1000000);fSize->setValue(20);
    fSizeUnit=new QComboBox(sizePage);fSizeUnit->setObjectName("AutoRunSizeUnit");fSizeUnit->addItem(tr("MB"),1000000LL);fSizeUnit->addItem(tr("GB"),1000000000LL);fSizeUnit->setCurrentIndex(1);
    sizeLayout->addWidget(fSizeEnabled);sizeLayout->addWidget(fSize,1);sizeLayout->addWidget(fSizeUnit);layout->addWidget(sizePage);
    auto *action=new QHBoxLayout;action->setSpacing(6);
    fAction=new QComboBox(this);fAction->setObjectName("AutoRunAction");fAction->addItems({tr("Stop"),tr("Next Run")});
    fRuns=new QSpinBox(this);fRuns->setObjectName("AutoRunMaximumRuns");fRuns->setRange(0,1000000);fRuns->setSpecialValueText(tr("Unlimited"));fRuns->setToolTip(tr("Total Runs in this automatic sequence, including the first Run. 0 means unlimited. Next Run waits at least 3 s after successful finalization, and requires verified Ready."));
    action->addWidget(fAction,1);action->addWidget(new QLabel(tr("Runs"),this));action->addWidget(fRuns,1);layout->addLayout(action);
    fApply=new QPushButton(tr("Apply"),this);auto *apply=fApply;apply->setObjectName("ApplyAutoRun");
    apply->setToolTip(tr("Apply conditions before Run start. Conditions stay locked until the Run or automatic sequence finishes; use Run stop to cancel."));apply->setFixedWidth(68);apply->setFixedHeight(24);
    header->addWidget(apply);
    fStatus=new QLabel(tr("Automatic Run off"),this);fStatus->setObjectName("AutoRunStatus");
    fStatus->setWordWrap(true);fStatus->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);
    fStatus->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);layout->addWidget(fStatus);
    for(auto *control:QList<QWidget *>{fAction,fTime,fSize,fTimeUnit,fSizeUnit,fRuns})control->setFixedHeight(28);
    connect(fTimeUnit,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{fTime->setMaximum(double(365LL*24*3600000)/fTimeUnit->currentData().toLongLong());});
    const auto changed=[this]{if(!fUpdating){fPending=true;fStatus->setText(tr("Changes pending · Apply"));}};
    connect(fTimeEnabled,&QCheckBox::toggled,this,[this,changed]{UpdateInputs();changed();});
    connect(fSizeEnabled,&QCheckBox::toggled,this,[this,changed]{UpdateInputs();changed();});
    connect(fTime,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[changed]{changed();});
    connect(fSize,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[changed]{changed();});
    connect(fRuns,QOverload<int>::of(&QSpinBox::valueChanged),this,[changed]{changed();});
    for(auto *combo:{fAction,fTimeUnit,fSizeUnit})connect(combo,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[changed]{changed();});
    connect(fAction,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{UpdateInputs();});
    connect(fEnabled,&QCheckBox::toggled,this,[this](bool enabled){UpdateInputs();if(!fUpdating)emit EnableRequested(enabled);});
    connect(apply,&QPushButton::clicked,this,[this]{const auto settings=Settings();QString error;if(settings.Validate(&error))fPending=false;emit ApplyRequested(settings);});UpdateInputs();
}
void AutoRunWidget::UpdateInputs() {
    fEnabled->setEnabled(true);fApply->setEnabled(!fLocked);
    fTimeEnabled->setEnabled(!fLocked && fEnabled->isChecked());fSizeEnabled->setEnabled(!fLocked && fEnabled->isChecked());
    fAction->setEnabled(!fLocked && fEnabled->isChecked());
    for(auto *control:QList<QWidget *>{fTime,fTimeUnit})control->setEnabled(!fLocked && fEnabled->isChecked() && fTimeEnabled->isChecked());
    for(auto *control:QList<QWidget *>{fSize,fSizeUnit})control->setEnabled(!fLocked && fEnabled->isChecked() && fSizeEnabled->isChecked());
    fRuns->setEnabled(!fLocked && fEnabled->isChecked() && fAction->currentIndex()==1);
}
AutoRunSettings AutoRunWidget::Settings() const {
    AutoRunSettings settings;settings.enabled=fEnabled->isChecked();settings.condition=fTimeEnabled->isChecked() ? (fSizeEnabled->isChecked() ? AutoRunSettings::Either : AutoRunSettings::Time) : (fSizeEnabled->isChecked() ? AutoRunSettings::FileSize : AutoRunSettings::NoCondition);
    settings.action=fAction->currentIndex()==0 ? AutoRunSettings::Stop : AutoRunSettings::NextRun;
    settings.durationMs=qRound64(fTime->value()*fTimeUnit->currentData().toLongLong());
    settings.fileBytes=quint64(qRound64(fSize->value()*fSizeUnit->currentData().toLongLong()));settings.maximumRuns=fRuns->value();return settings;
}
void AutoRunWidget::SetSettings(const AutoRunSettings &s) {
    const QScopedValueRollback<bool> updating(fUpdating,true);
    const QSignalBlocker enabled(fEnabled),time(fTimeEnabled),size(fSizeEnabled),action(fAction);
    fPending=false;fEnabled->setChecked(s.enabled);fTimeEnabled->setChecked(s.UsesTime());fSizeEnabled->setChecked(s.UsesSize());
    fAction->setCurrentIndex(s.action==AutoRunSettings::Stop ? 0 : 1);fRuns->setValue(s.maximumRuns);
    int timeUnit=s.durationMs%60000==0 ? 1 : 0;
    if(s.durationMs%3600000==0)timeUnit=2;
    fTimeUnit->setCurrentIndex(timeUnit);fTime->setValue(double(s.durationMs)/fTimeUnit->currentData().toLongLong());
    fSizeUnit->setCurrentIndex(s.fileBytes%1000000000ULL==0 ? 1 : 0);
    fSize->setValue(double(s.fileBytes)/fSizeUnit->currentData().toLongLong());UpdateInputs();
}
void AutoRunWidget::SetStatus(const QString &text,bool armed) {
    fStatus->setText(fPending ? tr("Changes pending · Apply") : text);fStatus->setToolTip(text);fStatus->setProperty("armed",armed);
}

void AutoRunWidget::SetLocked(bool locked) {fLocked=locked;UpdateInputs();}

void AutoRunWidget::SetEnabledState(bool enabled) {const QSignalBlocker block(fEnabled);fEnabled->setChecked(enabled);UpdateInputs();}
