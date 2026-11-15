#include "simulator.h"
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

Simulator::Simulator(QObject *parent) : QObject(parent)
{ connect(&fDataTimer, &QTimer::timeout, this, &Simulator::WriteData); }

bool Simulator::Start(const QString &root, int cobos, int eccPort, int sentryPort, int failStart,
                      int delayMs, quint64 maxGeneratedBytes, QString *error)
{
    const quint64 maximumAllowedBytes = 20ULL * 1024ULL * 1024ULL * 1024ULL;
    if (cobos < 1 || cobos > 32 || eccPort < 0 || sentryPort < 0 || eccPort + cobos > 65535 || sentryPort + cobos > 65535 || delayMs < 0 || delayMs > 60000 ||
        maxGeneratedBytes < 64ULL * 1024ULL || maxGeneratedBytes > maximumAllowedBytes) {
        *error = "Invalid CoBo count, port range, delay or generated-data budget."; return false;
    }
    fRoot = QDir(root).absolutePath(); fFailStart = failStart; fDelayMs = delayMs;
    fMaxGeneratedBytes = maxGeneratedBytes; fGeneratedBytes = 0; fBudgetReported = false; fWriteFault = false;
    if (!QDir().mkpath(fRoot)) { *error = "Cannot create simulator root."; return false; }
    fStates.fill(1, cobos + 1);
    fTransitions.fill(0, cobos + 1);
    QJsonArray nodes;
    for (int id = 0; id <= cobos; ++id) {
        const bool mutant = id == cobos;
        HttpServer *ecc = new HttpServer(this);
        if (!ecc->listen(QHostAddress::LocalHost, quint16(eccPort == 0 ? 0 : eccPort + id))) { *error = ecc->errorString(); return false; }
        connect(ecc, &HttpServer::RequestSignal, this, [this, id](QString method, QString path, QByteArray body, HttpConnection *connection) {
            if (path != "/") { connection->Reply(404, "{}"); return; }
            HandleEcc(id, method, body, connection);
        });
        fEcc.append(ecc);
        int nodeSentryPort = sentryPort == 0 ? 19080 : sentryPort + id; // MuTaNT 无 sentry；该字段仅保持配置格式有效。
        if (!mutant) {
            SentryConfig sc;
            sc.dataDir = QDir(fRoot).filePath(QString("node%1/incoming").arg(id));
            sc.port = sentryPort == 0 ? 0 : sentryPort + id; sc.settleMs = 100; sc.checkProcess = false;
            QDir().mkpath(sc.dataDir);
            SentryService *service = new SentryService(sc, this);
            if (!service->Start(error)) return false;
            nodeSentryPort = service->Port();
            fSentry.append(service);
        }
        if (!AccountExistingGeneratedBytes(id, error)) return false;
        QJsonObject node;
        node["id"] = id; node["role"] = mutant ? "mutant" : "cobo";
        node["name"] = mutant ? "MuTaNT (mock)" : QString("CoBo %1 (mock)").arg(id);
        node["host"] = "127.0.0.1"; node["ecc_port"] = int(ecc->serverPort()); node["sentry_port"] = nodeSentryPort;
        nodes.append(node);
    }
    QString configPath;
    if (!PrepareConfigurationWorkspace(cobos, nodes, &configPath, error)) return false;
    qInfo().noquote() << "Open in GetDAQ:" << configPath;
    qInfo() << "If changing node count or ports, use a new --root or update the existing envoy.yml.";
    fDataTimer.start(200); return true;
}

