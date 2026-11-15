#include "scanplancommands_p.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QSaveFile>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

bool SavePlanFile(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool ParseDecimal(const QString &text, unsigned int maximum, unsigned int *value)
{
    bool ok = false;
    const uint parsed = text.toUInt(&ok, 10);
    if (!ok || QString::number(parsed) != text || parsed > maximum) return false;
    *value = static_cast<unsigned int>(parsed);
    return true;
}

std::string ReadScanPlanFile(const QString &path, qint64 maximum)
{
    const QFileInfo info(path);
    if (info.isSymLink() || !info.isFile() || info.size() <= 0 || info.size() > maximum)
        throw std::runtime_error("Scan policy input is missing, linked, non-regular, empty, or oversized");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read scan policy input");
    const QByteArray bytes = file.read(maximum + 1);
    if (file.error() != QFile::NoError || bytes.size() != info.size() || bytes.size() > maximum)
        throw std::runtime_error("Scan policy input changed or could not be read completely");
    return bytes.toStdString();
}

bool SaveNewMetadataFile(const QString &path, const QByteArray &bytes, QString *error,
                         const QString &description)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
        if (error) *error = description + QStringLiteral(" output already exists or cannot be created: ") + file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        if (error) *error = description + QStringLiteral(" output write failed: ") + file.errorString();
        return false;
    }
    return true;
}

GetDAQ::SystemScanPlanSummary ValidateScanPlanDirectory(const QString &directory,
                                                         std::string *manifest)
{
    const QFileInfo dir(directory);
    const QString absolute = QDir::cleanPath(dir.absoluteFilePath());
    if (dir.isSymLink() || !dir.isDir() || dir.canonicalFilePath() != absolute)
        throw std::runtime_error("Scan plan must be a real, non-linked directory");
    *manifest = ReadScanPlanFile(QDir(absolute).filePath(QStringLiteral("scan-manifest.json")),
                                 1024 * 1024);
    return GetDAQ::ValidateSystemScanPlan(*manifest, [&](const std::string &name) {
        return ReadScanPlanFile(QDir(absolute).filePath(QString::fromLatin1(name.c_str())),
                                64LL * 1024 * 1024);
    });
}

}
