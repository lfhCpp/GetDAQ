#ifndef GETDAQ_CONSOLELOGSTORE_H
#define GETDAQ_CONSOLELOGSTORE_H

#include <QString>
#include <QByteArray>
#include <QFile>

// Owns the durable application log and the currently active connection log.
// Presentation, severity classification and connection state remain in MainWindow.
class ConsoleLogStore {
public:
    bool OpenApplicationLog();
    bool OpenConnectionLog();
    void CloseConnectionLog();
    void Append(const QByteArray &line);

    bool HasApplicationLog() const { return fApplication.isOpen(); }
    bool HasConnectionLog() const { return fConnection.isOpen(); }
    bool UsedFallbackDirectory() const { return fUsedFallback; }
    QString ApplicationLogPath() const { return fApplication.fileName(); }
    QString ConnectionLogPath() const { return fConnection.fileName(); }
    QString ErrorString() const { return fError; }

private:
    QFile fApplication;
    QFile fConnection;
    QString fApplicationDirectory;
    QString fError;
    bool fUsedFallback=false;
};

#endif
