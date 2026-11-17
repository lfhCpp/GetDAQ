#include "simulator.h"

#include <QDir>
#include <QFile>
#include <QDirIterator>
#include <QFileInfo>
#include <QDebug>

bool Simulator::AccountExistingGeneratedBytes(int id, QString *error)
{
    const QString incomingPath = QDir(fRoot).filePath(QString("node%1/incoming").arg(id));
    QDirIterator rawFiles(incomingPath,
        QDir::Files | QDir::System | QDir::Hidden, QDirIterator::Subdirectories);
    while (rawFiles.hasNext()) {
        const QFileInfo simulatedInfo(rawFiles.next());
        if (simulatedInfo.suffix() != "graw") continue;
        if (simulatedInfo.isSymLink() || !simulatedInfo.isFile()) {
            *error = "Existing simulator .graw output is not a regular, non-symlink file.";
            return false;
        }
        const qint64 existingBytes = simulatedInfo.size();
        if (existingBytes < 0 || quint64(existingBytes) > fMaxGeneratedBytes - fGeneratedBytes) {
            *error = "Existing simulator .graw files exceed the configured total data budget.";
            return false;
        }
        fGeneratedBytes += quint64(existingBytes);
    }
    return true;
}

void Simulator::WriteData()
{
    if (fWriteFault || fStates.isEmpty() || fStates.last() != 5) return;
    QVector<int> active;
    for (int id = 0; id < fStates.size() - 1; ++id) if (fStates[id] == 5) active.append(id);
    for (int activeIndex = 0; activeIndex < active.size(); ++activeIndex) {
        if (fGeneratedBytes >= fMaxGeneratedBytes) {
            fDataTimer.stop();
            if (!fBudgetReported) {
                fBudgetReported = true;
                qWarning() << "Simulator generated-data budget reached; no more bytes will be written. Stop the simulated Run explicitly.";
            }
            return;
        }
        const int id = active[activeIndex];
        QFile file(QDir(fRoot).filePath(QString("node%1/incoming/simulated.graw").arg(id)));
        if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            if (file.size() == 0) {
                const QByteArray marker("GETDAQ SIMULATED BYTES — NOT REAL GET/MFM FRAMES\n");
                const qint64 markerBytes = qMin<quint64>(quint64(marker.size()), fMaxGeneratedBytes - fGeneratedBytes);
                const qint64 written = file.write(marker.constData(), markerBytes);
                if (written != markerBytes) { fWriteFault = true; fDataTimer.stop(); qWarning().noquote() << "Simulator output write failed:" << file.errorString(); return; }
                fGeneratedBytes += quint64(written);
            }
            const quint64 remainingBudget = fMaxGeneratedBytes - fGeneratedBytes;
            if (remainingBudget) {
                const quint64 share = qMax<quint64>(1, remainingBudget / quint64(active.size() - activeIndex));
                const quint64 count = qMin<quint64>(64ULL * 1024ULL, share);
                const QByteArray payload(static_cast<int>(count), char(id));
                const qint64 written = file.write(payload);
                if (written != payload.size()) { fWriteFault = true; fDataTimer.stop(); qWarning().noquote() << "Simulator output write failed:" << file.errorString(); return; }
                fGeneratedBytes += quint64(written);
            }
        } else {
            fWriteFault = true; fDataTimer.stop(); qWarning().noquote() << "Cannot write simulator output:" << file.errorString(); return;
        }
    }
    if (fGeneratedBytes >= fMaxGeneratedBytes) {
        fDataTimer.stop();
        if (!fBudgetReported) {
            fBudgetReported = true;
            qWarning() << "Simulator generated-data budget reached; no more bytes will be written. Stop the simulated Run explicitly.";
        }
    }
}
