#include "scanplanoutput.h"

#include "systemscanplan.h"
#include "configurationsha256.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <stdexcept>

ScanPlanOutputSummary InspectScanPlanOutput(const QString &directory)
{
    ScanPlanOutputSummary result;
    const QString manifestPath = QDir(directory).filePath(QStringLiteral("scan-manifest.json"));
    const QFileInfo manifestInfo(manifestPath);
    if (manifestInfo.isSymLink() || !manifestInfo.isFile() || manifestInfo.size() <= 0 ||
        manifestInfo.size() > 1024 * 1024) {
        result.error = QStringLiteral("The plan manifest is missing, linked, non-regular, empty, or exceeds 1 MiB.");
        return result;
    }

    QFile manifest(manifestPath);
    if (!manifest.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("The plan manifest could not be opened.");
        return result;
    }
    const QByteArray manifestBytes = manifest.readAll();
    if (manifest.error() != QFile::NoError || manifestBytes.size() != manifestInfo.size()) {
        result.error = QStringLiteral("The plan manifest could not be read completely.");
        return result;
    }

    try {
        const GetDAQ::SystemScanPlanSummary validated = GetDAQ::ValidateSystemScanPlan(
            manifestBytes.toStdString(), [&](const std::string &name) {
                const QString artifactPath = QDir(directory).filePath(QString::fromLatin1(name.c_str()));
                const QFileInfo info(artifactPath);
                if (info.isSymLink() || !info.isFile() || info.size() <= 0 ||
                    info.size() > 64 * 1024 * 1024)
                    throw std::invalid_argument("Scan point artifact is missing, linked, non-regular, empty, or oversized");
                QFile artifact(artifactPath);
                if (!artifact.open(QIODevice::ReadOnly))
                    throw std::invalid_argument("Scan point artifact cannot be opened");
                const QByteArray bytes = artifact.readAll();
                if (artifact.error() != QFile::NoError || bytes.size() != info.size())
                    throw std::invalid_argument("Scan point artifact could not be read completely");
                return bytes.toStdString();
            });
        result.kind = QString::fromStdString(validated.kind);
        result.targetCoBo = QString::fromStdString(validated.targetCoBo);
        result.pointCount = static_cast<int>(validated.pointCount);
        result.presentFileCount = result.pointCount;
        result.planSha256 = QString::fromStdString(
            GetDAQ::ConfigurationSha256(manifestBytes.toStdString()));
    } catch (const std::exception &e) {
        result.error = QString::fromLocal8Bit(e.what());
    }
    return result;
}
