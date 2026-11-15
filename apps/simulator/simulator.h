#ifndef GETDAQ_SIMULATOR_H
#define GETDAQ_SIMULATOR_H
#include "httpserver.h"
#include "sentryservice.h"
#include <QVector>

class QJsonArray;

class Simulator : public QObject
{
    Q_OBJECT
public:
    explicit Simulator(QObject *parent = nullptr);
    bool Start(const QString &root, int cobos, int eccPort, int sentryPort, int failStart,
               int delayMs, quint64 maxGeneratedBytes, QString *error);
private:
    void HandleEcc(int id, QString method, QByteArray body, HttpConnection *connection);
    void WriteData();
    bool AccountExistingGeneratedBytes(int id, QString *error);
    bool PrepareConfigurationWorkspace(int cobos, const QJsonArray &nodes,
                                       QString *configPath, QString *error);
    QVector<int> fStates;
    QVector<int> fTransitions;
    QVector<HttpServer *> fEcc;
    QVector<SentryService *> fSentry;
    QString fRoot;
    int fFailStart = -1;
    int fDelayMs = 0;
    quint64 fMaxGeneratedBytes = 0;
    quint64 fGeneratedBytes = 0;
    bool fBudgetReported = false;
    bool fWriteFault = false;
    QTimer fDataTimer;
};
#endif
