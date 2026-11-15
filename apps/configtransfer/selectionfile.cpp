#include "selectionfile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

bool PublishNewSelectionFile(const QString &path, const QByteArray &bytes,
                             QString *error)
{
    const QFileInfo destination(path);
    if (path.isEmpty() || destination.fileName().isEmpty() || bytes.isEmpty()) {
        if (error) *error = QStringLiteral("Selection output path and contents are required");
        return false;
    }
    const QDir directory(destination.absolutePath());
    if (!directory.exists()) {
        if (error) *error = QStringLiteral("Selection output directory does not exist");
        return false;
    }
    // Fail before creating a temporary file when the destination already
    // exists. QFile::rename correctly refuses replacement, but platform file
    // APIs differ in how a directory destination is handled after the failed
    // rename. The final rename below remains the atomic no-replace gate for a
    // destination created concurrently after this check.
    if (destination.exists() || destination.isSymLink()) {
        if (error) *error = QStringLiteral("Cannot publish selection without replacing an existing file");
        return false;
    }

    QTemporaryFile temporary(directory.filePath(
        QStringLiteral(".%1.XXXXXX").arg(destination.fileName())));
    if (!temporary.open()) {
        if (error) *error = temporary.errorString();
        return false;
    }
    if (temporary.write(bytes) != bytes.size() || !temporary.flush() ||
        !temporary.setPermissions(QFile::ReadOwner)) {
        if (error) *error = temporary.errorString().isEmpty()
            ? QStringLiteral("Could not write a complete private selection file")
            : temporary.errorString();
        return false;
    }
    temporary.close();
    if (!temporary.rename(path)) {
        if (error) *error = QStringLiteral("Cannot publish selection without replacing an existing file");
        return false;
    }
    temporary.setAutoRemove(false);
    if (error) error->clear();
    return true;
}
