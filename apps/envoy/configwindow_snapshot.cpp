#include "configwindow.h"
#include "effectiveconfigurationsnapshot.h"
#include "systemconfigurationpublisher.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFutureWatcher>
#include <QLabel>
#include <QtConcurrent/QtConcurrentRun>
#include <QPushButton>
#include <QPlainTextEdit>

bool ConfigWindow::ExportEffectiveSnapshot(const QString &path,QString *error)
{
    QJsonObject snapshot;
    if (!EffectiveConfigurationSnapshot::Capture(fDocument,&snapshot,error) || !EffectiveConfigurationSnapshot::Save(snapshot,path,error)) return false;
    fStatus->setText(tr("Saved local effective snapshot: %1\nEffective SHA-256: %2\nThis records local configuration; hardware loading is not verified.").arg(path,snapshot.value("effective_sha256").toString()));
    return true;
}
bool ConfigWindow::PublishSystemConfiguration(const QString &profile,const QString &directory,QString *error)
{
    QJsonObject result; if (!SystemConfigurationPublisher::Publish(fDocument,profile,directory,&result,error)) return false;
    fStatus->setText(tr("Built local publication: %1\nManifest SHA-256: %2\nHardware loading is not verified.").arg(result.value("manifest_file").toString(),result.value("manifest_sha256").toString())); return true;
}
bool ConfigWindow::CompareEffectiveSnapshot(const QString &path,QString *error)
{
    fSnapshotReview->clear(); fSnapshotReview->hide(); QJsonObject before,after,report;
    if (!EffectiveConfigurationSnapshot::Load(path,&before,error) || !EffectiveConfigurationSnapshot::Capture(fDocument,&after,error)
        || !EffectiveConfigurationSnapshot::Compare(before,after,&report,error)) return false;
    ShowSnapshotReport(report); return true;
}
void ConfigWindow::ShowSnapshotReport(const QJsonObject &report)
{
    fSnapshotReview->setPlainText(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented))); fSnapshotReview->show();
    fStatus->setText(tr("Compared saved baseline with the loaded local configuration.\nEffective parameters changed: %1; snapshot/source record changed: %2\nScalar changes: %3; object changes: %4; source changes: %5. No configuration file or hardware was changed.")
        .arg(report.value("effective_changed").toBool() ? tr("yes") : tr("no"),report.value("snapshot_changed").toBool() ? tr("yes") : tr("no"))
        .arg(report.value("scalar_changes").toArray().size()).arg(report.value("object_changes").toArray().size()).arg(report.value("source_changes").toArray().size()));
}
void ConfigWindow::StartSnapshotTask(const QString &path,bool compare,QPushButton *exportButton,QPushButton *compareButton)
{
    exportButton->setEnabled(false); compareButton->setEnabled(false); fSnapshotReview->clear(); fSnapshotReview->hide();
    fStatus->setText(tr("Reading and checking effective configuration in the background..."));
    const int revision=fRevision; const XcfgDocument document=fDocument;
    typedef QPair<QJsonObject,QString> Result;
    auto watcher=new QFutureWatcher<Result>(this);
    connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,revision,path,compare,exportButton,compareButton] {
        const auto result=watcher->result(); watcher->deleteLater(); exportButton->setEnabled(true); compareButton->setEnabled(true);
        if (!result.second.isEmpty()) { fStatus->setText(result.second); return; }
        if (revision!=fRevision) {
            fStatus->setText(compare ? tr("Configuration changed during comparison. Compare again.") : tr("Snapshot saved for the earlier loaded configuration: %1. Export again for the current configuration.").arg(path)); return;
        }
        if (compare) ShowSnapshotReport(result.first);
        else fStatus->setText(tr("Saved local effective snapshot: %1\nEffective SHA-256: %2\nHardware loading is not verified.").arg(path,result.first.value("effective_sha256").toString()));
    });
    watcher->setFuture(QtConcurrent::run([document,path,compare]() -> Result {
        QJsonObject current,before,report; QString error;
        if (!EffectiveConfigurationSnapshot::Capture(document,&current,&error)) return qMakePair(QJsonObject(),error);
        if (compare) {
            if (!EffectiveConfigurationSnapshot::Load(path,&before,&error) || !EffectiveConfigurationSnapshot::Compare(before,current,&report,&error)) return qMakePair(QJsonObject(),error);
            return qMakePair(report,QString());
        }
        if (!EffectiveConfigurationSnapshot::Save(current,path,&error)) return qMakePair(QJsonObject(),error);
        return qMakePair(current,QString());
    }));
}
