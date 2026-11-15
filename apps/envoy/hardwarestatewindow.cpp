#include "hardwarestatewindow.h"
#include "ui_hardwarestatewindow.h"
#include "hardwarestateprotocol.h"
#include <QFile>
#include <QTimer>
#include <QHeaderView>
#include <stdexcept>
HardwareStateWindow::HardwareStateWindow(const EccSystemClientOptions &options,QWidget *parent)
    : QDialog(parent),ui(new Ui::HardwareStateWindow),fHttp(new HttpClient(this)) {
    ui->setupUi(this);ui->address->setText(options.address.isEmpty() ? "127.0.0.1" : options.address);ui->port->setValue(options.port ? options.port : 26000);ui->credential->setText(options.credentialFile);ui->service->setText(options.serviceId);ui->snapshot->setText(options.snapshotSha256);
    ui->nodes->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    connect(ui->inspect,&QPushButton::clicked,this,&HardwareStateWindow::Inspect);
    for(auto field:{ui->address,ui->credential,ui->service,ui->snapshot})connect(field,&QLineEdit::textChanged,this,&HardwareStateWindow::Invalidate);
    connect(ui->port,QOverload<int>::of(&QSpinBox::valueChanged),this,&HardwareStateWindow::Invalidate);
    auto timer=new QTimer(this);timer->setInterval(1000);connect(timer,&QTimer::timeout,this,[this]{if(fFresh.isValid() && fFresh.elapsed()>6000){fFresh.invalidate();ui->nodes->setRowCount(0);ui->status->setText(tr("Observation expired. Values withdrawn; explicitly inspect again. No control action was sent."));}});timer->start();
}
HardwareStateWindow::~HardwareStateWindow(){fHttp->Cancel();delete ui;}
QUrl HardwareStateWindow::Url(const QString &path) const {QUrl u;u.setScheme("http");u.setHost(ui->address->text().trimmed());u.setPort(ui->port->value());u.setPath(path);return u;}
QMap<QByteArray,QByteArray> HardwareStateWindow::Headers(){QFile file(ui->credential->text());if(!file.open(QIODevice::ReadOnly) || file.size()>4096)throw std::runtime_error("Cannot read bounded local credential");const auto token=file.readAll().trimmed();if(token.size()<32 || token.contains('\n') || token.contains('\r') || (!fCredential.isEmpty() && token!=fCredential))throw std::runtime_error("Credential invalid/changed; reselect endpoint");fCredential=token;return {{"Authorization","Bearer "+token}};}
void HardwareStateWindow::Invalidate(){++fGeneration;fHttp->Cancel();fBusy=false;fFresh.invalidate();fCredential.clear();ui->nodes->setRowCount(0);ui->inspect->setEnabled(true);ui->status->setText(tr("Endpoint selection changed. No hardware values are confirmed."));}
void HardwareStateWindow::Failure(const QString &message){fBusy=false;fFresh.invalidate();ui->nodes->setRowCount(0);ui->inspect->setEnabled(true);ui->status->setText(message);}
void HardwareStateWindow::Inspect(){if(fBusy)return;ui->nodes->setRowCount(0);fFresh.invalidate();fBusy=true;ui->inspect->setEnabled(false);const auto generation=fGeneration;
    try{const auto headers=Headers();fHttp->Request(Url("/v1/identity"),{},"",6000,[this,generation](const QByteArray &body,const QString &error){if(generation!=fGeneration)return;
        try{if(!error.isEmpty())throw std::runtime_error(error.toStdString());const auto identity=GetDAQ::DecodeEccControlIdentity(body.toStdString());if(identity.serviceId!=ui->service->text().toStdString() || identity.snapshotSha256!=ui->snapshot->text().toStdString())throw std::runtime_error("Selected service/snapshot identity differs");
            fHttp->Request(Url("/v1/hardware-state"),{},"",6000,[this,generation,identity](const QByteArray &bytes,const QString &failure){if(generation!=fGeneration)return;
                try{if(!failure.isEmpty())throw std::runtime_error(failure.toStdString());const auto o=GetDAQ::DecodeHardwareStateObservation(bytes.toStdString());if(o.instanceId!=identity.instanceId || o.snapshotSha256!=identity.snapshotSha256 || o.configurationSha256!=identity.configurationSha256 || o.artifactSha256!=identity.artifactSha256)throw std::runtime_error("Observed hardware state belongs to another binding");
                    ui->nodes->setRowCount(int(o.nodes.size()));int row=0;for(const auto &n:o.nodes){QStringList values{QString::fromStdString(n.id),QString::fromStdString(n.role),n.available ? tr("Observed") : tr("Unavailable")};QString lifecycle;for(const auto &board:o.coboLifecycle)if(board.id==n.id){lifecycle=QString::fromStdString(board.state);break;}values.append(lifecycle);for(const auto &field:{"firmwareEnable","intEnable","initDone","mutantMode","run"}){const auto found=n.values.find(field);values.append(found==n.values.end() ? QString() : QString::number(found->second));}values.append(n.available ? QString::number(qulonglong(n.observedAtUnixMs)) : QString());values.append(QString::fromStdString(n.message));for(int c=0;c<values.size();++c)ui->nodes->setItem(row,c,new QTableWidgetItem(values[c]));++row;}
                    fBusy=false;fFresh.start();ui->inspect->setEnabled(true);const auto lifecycleEvidence=o.coboLifecycleSource=="live-controller-cache" ? tr("CoBo phase comes from the live controller cache, not hardware readback.") : (o.coboLifecycleSource=="controller-baseline" ? tr("CoBo phase comes from the controller baseline; it is not a persistent per-board state or hardware readback.") : tr("CoBo lifecycle is unavailable for this snapshot."));ui->status->setText(tr("ECC state %1 / operation %2 / instance %3. %4 %5 Loading, sender closure and global ownership remain unverified; no recovery or write was issued.").arg(o.controllerState).arg(QString::fromStdString(o.controllerOperationId),QString::fromStdString(o.instanceId),lifecycleEvidence,o.controllerOutcomeUnknown || o.controllerNeedsReconciliation ? tr("Parent UNKNOWN/reconciliation remains.") : (o.busy ? tr("Backend busy; no register query issued.") : (o.consistent ? tr("Fixed control samples observed.") : tr("Controller changed; all samples withdrawn.")))));
                }catch(const std::exception &e){Failure(QString::fromUtf8(e.what()));}},Headers());
        }catch(const std::exception &e){Failure(QString::fromUtf8(e.what()));}},headers);
    }catch(const std::exception &e){Failure(QString::fromUtf8(e.what()));}
}
