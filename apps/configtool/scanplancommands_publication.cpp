#include "scanplancommands_p.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

void ValidateGeneratedScanPlanTarget(const QString &targetPath) {
    const QFileInfo target(targetPath);
    const QString parentPath = target.absoluteDir().absolutePath();
    if (!QDir(parentPath).exists())
        throw std::runtime_error("Output parent directory does not exist");
    if (QFileInfo::exists(targetPath) || target.isSymLink())
        throw std::runtime_error("Output directory already exists; refusing to overwrite");
}

void PublishGeneratedScanPlan(const QString &targetPath, const QString &stagingName,
                              const std::function<QJsonObject(const QString &)> &writePlan) {
    const QFileInfo target(targetPath);
    const QString parentPath = target.absoluteDir().absolutePath();
    QTemporaryDir staging(parentPath + QStringLiteral("/.getdaq-%1-XXXXXX").arg(stagingName));
    if (!staging.isValid())
        throw std::runtime_error("Could not create private staging directory");

    const QJsonObject manifest = writePlan(staging.path());
    QString error;
    if (!SavePlanFile(QDir(staging.path()).filePath(QStringLiteral("scan-manifest.json")),
                      QJsonDocument(manifest).toJson(QJsonDocument::Indented), &error))
        throw std::runtime_error(error.toStdString());
    if (QFileInfo::exists(targetPath) || target.isSymLink() ||
        !QDir().rename(staging.path(), targetPath))
        throw std::runtime_error("Could not publish scan directory without replacing an existing path");
}

}
