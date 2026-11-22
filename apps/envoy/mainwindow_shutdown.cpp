#include "mainwindow.h"
#include "applicationinstance.h"
#include "offlinedatatoolswindow.h"
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QDir>
#ifdef Q_OS_LINUX
#include "backendshutdown.h"
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>
#include <QJsonArray>
#include <QMetaObject>
#endif
void MainWindow::FinishExit(){
    fClosing=true;
    if(fWaveformViewer)fWaveformViewer->close();
    if(fDataTools)fDataTools->close();
    setEnabled(false);emit ShutdownSignal();
}
void MainWindow::RequestExit(){
#ifdef Q_OS_LINUX
    fClosing=true;setEnabled(false);ShowLogSlot(tr("Checking managed ECC services before exit…"));
    const auto project=fConfig.deploymentProject.isEmpty()?QSettings().value(StartupConfigurationKey()+"/distributed-project").toString()+"/project.json":fConfig.deploymentProject;
    const auto cache=QSettings().value(StartupConfigurationKey()+"/slave-cache-directory",DefaultNodeCacheDirectory()).toString();
    auto *watcher=new QFutureWatcher<QJsonObject>(this);
    connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,cache]{
        const auto plan=watcher->result();watcher->deleteLater();setEnabled(true);
        const auto targets=plan["targets"].toArray(),errors=plan["errors"].toArray();
        if(targets.isEmpty() && errors.isEmpty()){
            if((fActive || fBusy || fRemoteRunning) && QMessageBox::question(this,tr("Acquisition may still be active"),tr("Closing does not stop remote acquisition. An interrupted journal will be kept. Close the controller anyway?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){fClosing=false;return;}
            FinishExit();return;
        }
        QStringList details;for(const auto &value:targets){const auto target=value.toObject();details<<target["node"].toObject()["name"].toString()+" / "+target["instance"].toObject()["instance_id"].toString();}for(const auto &error:errors)details<<error.toString();
        QMessageBox prompt(QMessageBox::Question,tr("Managed backend services"),tr("GetDAQ-managed ECC / data services are still running or need inspection.\n\nStop them before exiting, keep them running, or cancel exit?\nStopping services requires acquisition to be stopped first. Existing data and configuration are retained."),QMessageBox::NoButton,this);prompt.setObjectName("ManagedBackendExitPrompt");prompt.setDetailedText(details.join('\n'));
        auto *stop=prompt.addButton(tr("Stop ECC + data and exit"),QMessageBox::AcceptRole);stop->setObjectName("StopBackendsAndExit");stop->setEnabled(errors.isEmpty());auto *force=prompt.addButton(tr("Force terminate backends and exit…"),QMessageBox::DestructiveRole);force->setObjectName("ForceBackendsAndExit");force->setEnabled(!targets.isEmpty());auto *keep=prompt.addButton(tr("Keep services running and exit"),QMessageBox::DestructiveRole);keep->setObjectName("KeepBackendsAndExit");auto *cancel=prompt.addButton(QMessageBox::Cancel);cancel->setObjectName("CancelBackendExit");prompt.setDefaultButton(cancel);prompt.setEscapeButton(cancel);prompt.exec();
        if(prompt.clickedButton()==keep){FinishExit();return;}
        const bool forceExit=prompt.clickedButton()==force;
        if(prompt.clickedButton()!=stop && !forceExit){fClosing=false;return;}
        if(forceExit && QMessageBox::warning(this,tr("Force terminate managed backends"),tr("Terminate the listed GetDAQ-managed ECC and data processes even if a Run is active or ECC is unresponsive? After a 10-second graceful attempt, remaining verified processes will be killed.\n\nThis does not confirm that CoBo acquisition or pulsing has stopped. Pending raw data may be lost. Journals will be retained for inspection.\nServices that could not be inventoried cannot be terminated. No other processes will be killed."),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){fClosing=false;return;}
        if(!forceExit && (fActive || fBusy || fRemoteRunning)){fClosing=false;QMessageBox::information(this,tr("Stop acquisition first"),tr("Stop the Run and wait for completion before closing ECC and data services. You can also exit while keeping them running."));return;}
        emit CancelAutomaticRunSignal(tr("Automatic Run canceled: stopping managed backends for exit."));
        setEnabled(false);auto *stopping=new QFutureWatcher<QString>(this);
        connect(stopping,&QFutureWatcher<QString>::finished,this,[this,stopping]{const auto error=stopping->result();stopping->deleteLater();if(error.isEmpty()){FinishExit();return;}setEnabled(true);fClosing=false;ShowLogSlot(tr("Backend stop incomplete: %1").arg(error));QMessageBox::warning(this,tr("Backend stop incomplete"),tr("%1\nThe main window remains open. Review service status, or close again and choose to keep services running.").arg(error));});
        stopping->setFuture(QtConcurrent::run([plan,cache,forceExit]{try{Deployment::OperationScope scope(nullptr,120000);StopManagedBackends(plan,cache+"/exit-operations",{},forceExit);return QString();}catch(const std::exception &error){return QString::fromUtf8(error.what());}}));
    });
    watcher->setFuture(QtConcurrent::run([project,cache]{try{Deployment::OperationScope scope(nullptr,15000);return ManagedBackendExitPlan(project=="/project.json"?QString():project,cache);}catch(const std::exception &error){return QJsonObject{{"errors",QJsonArray{QString::fromUtf8(error.what())}}};}}));
#else
    if((fActive || fBusy || fRemoteRunning) && QMessageBox::question(this,tr("Acquisition may still be active"),tr("Closing does not stop remote acquisition. Close the controller anyway?"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
    FinishExit();
#endif
}
