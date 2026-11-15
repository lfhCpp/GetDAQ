#include "mainwindow.h"
#include "configurationreadbackwindow.h"
#include "eccprotocol.h"
#include <QColor>
#include <QLabel>
#include <QTableWidget>
#include <stdexcept>

void MainWindow::ShowOwnershipSlot(QByteArray body,QString error,quint64 generation)
{
    if (generation!=fOwnershipGeneration || !fOwnershipConnected || !fConfig.nativeEcc) return;
    fOwnershipBody=error.isEmpty() ? body : QByteArray();fOwnershipError=error;
    fOwnershipObservedAt=fOwnershipBody.isEmpty() ? -1 : fOwnershipClock.elapsed();RenderOwnership();
}

void MainWindow::ShowConfigureReadbacksSlot(QByteArray body,QString error,quint64 generation)
{
    if(generation!=fOwnershipGeneration || !fOwnershipConnected || !fConfig.nativeEcc)return;
    if(fReadbacksView)fReadbacksView->Observe(body,error,generation);
}

void MainWindow::RenderOwnership()
{
    if (!fOwnershipView) return;
    QVector<QPair<QString,QString>> rows;
    rows.append({tr("Protection scope"),tr("Cooperating ECC services on one host")});
    rows.append({tr("Board-global / cross-host control"),tr("Not verified")});
    rows.append({tr("Hardware configuration loaded"),tr("Not verified by this observation")});
    QColor color("#a87600");
    try {
        if (fOwnershipBody.isEmpty()) rows.prepend({tr("Observation"),fOwnershipError.isEmpty() ? tr("Waiting for a bound ECC response") : fOwnershipError});
        else {
            const auto ownership=GetDAQ::DecodeEccControlOwnership(fOwnershipBody.toStdString());
            if (QString::fromStdString(ownership.snapshotSha256)!=fConfig.nativeClient.snapshotSha256)
                throw std::runtime_error("Ownership response does not match the selected ECC configuration");
            const bool stale=fOwnershipObservedAt<0 || fOwnershipClock.elapsed()-fOwnershipObservedAt>6000;
            QString state;
            if (stale) state=tr("Observation expired; reservation is unconfirmed");
            else if (ownership.faultLatched) {state=tr("Reservation changed; reconciliation required");color=QColor("#b00020");}
            else if (!ownership.enabled) state=tr("Host reservation protection is disabled");
            else {state=tr("Host reservation held at the last ECC check");color=QColor("#087f23");}
            rows.prepend({tr("Observation"),state});
            rows.append({tr("Received"),tr("%1 seconds ago; ECC reports cached state").arg((fOwnershipClock.elapsed()-fOwnershipObservedAt)/1000)});
            rows.append({tr("ECC instance"),QString::fromStdString(ownership.instanceId)});
            rows.append({tr("Bound configuration"),QString::fromStdString(ownership.snapshotSha256)});
            rows.append({tr("Shared directory"),ownership.enabled ? QString::fromStdString(ownership.directory) : tr("Not enabled")});
            QStringList addresses;for (const auto &resource:ownership.resources) addresses.append(QString::fromStdString(resource));
            rows.append({tr("Reserved target addresses"),addresses.isEmpty() ? tr("None") : addresses.join(", ")});
        }
    } catch (const std::exception &error) {fOwnershipBody.clear();fOwnershipObservedAt=-1;rows.prepend({tr("Observation"),tr("Invalid ownership response: %1").arg(QString::fromUtf8(error.what()))});}
    fOwnershipView->setRowCount(rows.size());
    for (int i=0;i<rows.size();++i) {
        fOwnershipView->setItem(i,0,new QTableWidgetItem(rows[i].first));
        auto item=new QTableWidgetItem(rows[i].second);if (i==0) item->setForeground(color);
        fOwnershipView->setItem(i,1,item);
    }
}

void MainWindow::ShowConfigurationSlot(QByteArray body,QString error)
{
    if (!fConfig.nativeEcc || !fConnected) {body.clear();error.clear();}
    fConfigurationBody=body;fConfigurationError=error;
    QList<QPair<QString,QString>> rows;
    try {
        if (body.isEmpty()) {
            fConfigurationSummary->setText(tr("ECC configuration: not observed"));rows.append({tr("Observation"),error.isEmpty() ? tr("Waiting for a bound ECC response") : error});
        } else {
            const auto c=GetDAQ::DecodeEccControlConfiguration(body.toStdString());
            if (c.snapshotSha256!=fConfig.nativeClient.snapshotSha256.toStdString() || c.inputBindingSha256!=fConfig.nativeClient.inputBindingSha256.toStdString())
                throw std::runtime_error("Displayed ECC configuration differs from this deployment");
            fConfigurationSummary->setText(c.hasSelection ? tr("ECC selected version %1 · hardware load unverified").arg(QString::fromStdString(c.selection.registration.publicationSha256).left(12)) : tr("ECC startup artifact accepted · hardware load unverified"));
            rows.append({tr("ECC instance"),QString::fromStdString(c.instanceId)});rows.append({tr("Parent snapshot"),QString::fromStdString(c.snapshotSha256)});
            rows.append({tr("Fixed input binding"),QString::fromStdString(c.inputBindingSha256)});rows.append({tr("Configuration parameters"),QString::fromStdString(c.configurationSha256)});
            rows.append({tr("Experiment artifact"),QString::fromStdString(c.artifactSha256)});rows.append({tr("Repository selection"),c.hasSelection ? tr("Accepted at startup") : tr("No reviewed repository selection")});
            if (c.hasSelection) {
                const auto &s=c.selection;rows.append({tr("Repository identity"),QString::fromStdString(s.registration.repositoryId)});rows.append({tr("Publication version"),QString::fromStdString(s.registration.publicationSha256)});
                rows.append({tr("Registration receipt"),QString::fromStdString(s.registrationSha256)});rows.append({tr("Manifest on ECC host"),QString::fromStdString(s.registration.manifestFile)});
                rows.append({tr("Effective parameters"),QString::fromStdString(s.registration.effectiveSha256)});rows.append({tr("Source snapshot"),QString::fromStdString(s.registration.snapshotSha256)});
            }
            rows.append({tr("Live file health"),tr("Not checked by this cached response")});rows.append({tr("Hardware loading"),tr("Not verified")});
            if (!error.isEmpty()) rows.append({tr("Latest query"),tr("Unavailable; showing previously accepted metadata: %1").arg(error)});
        }
    } catch (const std::exception &e) {fConfigurationBody.clear();fConfigurationSummary->setText(tr("ECC configuration: invalid observation"));rows.append({tr("Error"),QString::fromUtf8(e.what())});}
    if (fConfigurationView) {
        fConfigurationView->setRowCount(rows.size());for (int i=0;i<rows.size();++i) {fConfigurationView->setItem(i,0,new QTableWidgetItem(rows[i].first));fConfigurationView->setItem(i,1,new QTableWidgetItem(rows[i].second));}
    }
}
