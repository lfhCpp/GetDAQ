#ifndef RUNRECORDER_P_H
#define RUNRECORDER_P_H

#include "runrecorder.h"
#include <QSaveFile>

namespace RunRecorderInternal {
inline bool WriteRunFile(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = file.errorString(); return false;
    }
    return true;
}
bool SnapshotConfiguration(const RunConfig &config, QString *error, bool *missingFile = nullptr);
}

#endif
