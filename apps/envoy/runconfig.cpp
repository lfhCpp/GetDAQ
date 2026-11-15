#include "runconfig.h"
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSet>
#include <limits>
#include <QHostAddress>



RunConfig RunConfig::Default()
{
    RunConfig c;
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    c.configDir = QDir(root).filePath("get-configs");
    c.outputDir = QDir(root).filePath("runs");
    const QStringList names = {"Target Gas", "Beam", "Energy (MeV/U)", "Pressure (Torr)",
        "B-Field (T)", "V_THGEM (V)", "V_MM (V)", "V_Cathode (kV)", "E-Drift (V)",
        "E-Trans (V)", "GET Freq. (MHz)"};
    for (const QString &name : names) c.fields.insert(name, QString());
    for (int i = 0; i < 12; ++i) {
        NodeConfig n;
        n.id = i;
        n.mutant = i == 11;
        n.name = n.mutant ? "MuTaNT" : QString("CoBo %1").arg(i);
        n.host = QString("192.168.41.%1").arg(n.mutant ? 1 : 60 + i);
        c.nodes.append(n);
    }
    return c;
}

QString NodeConfig::ConfigId(const QString &kind, const QString &experiment) const
{
    const QString explicitId = kind == "describe" ? describeId : (kind == "prepare" ? prepareId : configureId);
    if (!explicitId.isEmpty()) return explicitId;
    return kind == "describe" && !mutant ? QString("cobo%1").arg(id) : experiment;
}

bool RunConfig::Validate(QString *error) const
{
    QString problem;
    if(!autoRun.Validate(error))return false;
    if(nativeEcc && autoRun.enabled && autoRun.UsesSize() && !nativeClient.receiverStatusEnabled) {
        *error="Automatic file-size control requires native_receiver_status; enable receiver observations or choose time.";return false;
    }
    if (nativeCatalog && !nativeEcc) { *error="Native Run catalog requires whole-system native ECC mode."; return false; }
    if (nativeClient.alarmStatusEnabled && !nativeEcc) { *error="Native alarm cache requires whole-system native ECC mode."; return false; }
    if (nativeClient.receiverStatusEnabled && !nativeEcc) { *error="Receiver observation requires whole-system native ECC mode."; return false; }
    if(nativeClient.sourceStatusEnabled && !nativeEcc) {*error="Source observation requires whole-system native ECC mode.";return false;}
    if(nativeClient.receiverFailureStatusEnabled && !nativeEcc) {*error="Receiver failure cache requires whole-system native ECC mode.";return false;}
    // 实验名同时用于 GET 配置 ID 和目录，限制路径字符以避免越界。
    if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(experiment).hasMatch())
        problem = "Experiment must contain 1–80 letters, digits, '_' or '-', starting with a letter/digit.";
    else if (runNumber < 0 || runNumber == std::numeric_limits<int>::max()) problem = "Invalid run number.";
    else if (nodes.isEmpty() || nodes.size() > 256) problem = "Configure at least one CoBo (maximum 256 nodes).";
    else if (pollMs < 50 || timeoutMs < 100 || catalogTimeoutMs < 100 || stepTimeoutMs < timeoutMs || settleMs < 0)
        problem = "Invalid polling, request, transition or settling timeout.";
    else if (outputDir.isEmpty() || configDir.isEmpty()) problem = "Output and GET configuration directories must be set.";
    for (int ms : {pollMs, timeoutMs, catalogTimeoutMs, stepTimeoutMs, settleMs})
        if (ms > 3600000) problem = "Timeouts must not exceed one hour.";
    if(eccName.size()>128 || eccName.contains(QRegularExpression("[\\x00-\\x1f\\x7f]")))problem="Invalid ECC connection name.";
    QSet<int> ids;
    QSet<QString> endpoints;
    QSet<QString> nativeSources;
    int mutants = 0;
    for (const NodeConfig &n : nodes) {
        if (n.mutant) ++mutants;
        const QString endpoint = n.host.toLower() + ":" + QString::number(n.eccPort);
        if (n.id < 0 || ids.contains(n.id) || (!nativeEcc && endpoints.contains(endpoint))) problem = "Duplicate/invalid node ID or ECC endpoint.";
        if (!nativeEcc && (n.host.trimmed().isEmpty() || n.host.contains('/') || n.host.contains(' '))) problem = "Use a hostname or IP address without an URL scheme.";
        if (nativeEcc && (!n.mutant || !n.nativeSenderId.isEmpty() || !n.nativeAgentId.isEmpty())) {
            if (n.nativeSenderId.isEmpty() || n.nativeAgentId.isEmpty() || nativeSources.contains(n.nativeSenderId) ||
                n.nativeSenderId.toUtf8().size()>128 || n.nativeAgentId.toUtf8().size()>128 || n.nativeSenderId.contains(QRegularExpression("[\\x00-\\x1f\\x7f]")) || n.nativeAgentId.contains(QRegularExpression("[\\x00-\\x1f\\x7f]")))
                problem = "Native mode requires unique sender IDs and explicit data agent IDs.";
            nativeSources.insert(n.nativeSenderId);
        }
        if(!nativeEcc && n.soapNamespace!="urn:ecc" && n.soapNamespace!="urn:http://localhost:8083/") problem="Unsupported ECC SOAP namespace.";
        for (int p : {n.eccPort, n.sentryPort})
            if (p < 1 || p > 65535) problem = "Ports must be in 1–65535.";
        for (int p : {n.routerPort, n.exporterPort})
            if (p < 0 || p > 65535) problem = "Data ports must be 0 (disabled) or 1–65535.";
        for (const QString &host : {n.routerHost, n.exporterHost, n.sentryHost})
            if (host.contains('/') || host.contains(QRegularExpression("\\s"))) problem = "Invalid data receiver hostname.";
        ids.insert(n.id);
        endpoints.insert(endpoint);
    }
    if (mutants > (nativeEcc ? 3 : 1) || mutants == nodes.size()) problem = "Configure at least one CoBo and a supported MuTaNT count.";
    if (nativeEcc) {
        const QHostAddress address(nativeClient.address);
        const QRegularExpression digest("^[0-9a-f]{64}$");
        if (simulation || address.protocol()!=QAbstractSocket::IPv4Protocol || address.toString()!=nativeClient.address || address==QHostAddress::AnyIPv4 ||
            nativeClient.port<1 || nativeClient.port>65535 || nativeClient.serviceId.isEmpty() || nativeClient.clientId.isEmpty() ||
            !digest.match(nativeClient.snapshotSha256).hasMatch() || !digest.match(nativeClient.inputBindingSha256).hasMatch() || !digest.match(nativeClient.clientEpoch).hasMatch() ||
            !QDir::isAbsolutePath(nativeClient.credentialFile) || !QDir::isAbsolutePath(nativeRecordDir) ||
            QDir::cleanPath(nativeRecordDir)!=nativeRecordDir || nativeClient.timeoutMs<100 || nativeClient.timeoutMs>60000 || nativeClient.statusMaxAgeMs<100 || nativeClient.statusMaxAgeMs>3600000)
            problem = "Native ECC requires explicit non-simulation endpoint, identity, SHA/session and private record/credential paths.";
    }
    if (error) *error = problem;
    return problem.isEmpty();
}

QString RunConfig::RunDirectory() const
{
    return QDir(outputDir).filePath(QString("%1/%2/run_%3").arg(simulation ? "simulation" : "hardware", experiment)
                                  .arg(runNumber, 4, 10, QLatin1Char('0')));
}

QString StateName(int state)
{
    switch (state) {
    case 0: return "Off";
    case 1: return "Idle";
    case 2: return "Described";
    case 3: return "Prepared";
    case 4: return "Ready";
    case 5: return "Running";
    case 6: return "Paused";
    default: return "Error";
    }
}

QString TransitionName(int transition)
{
    switch (transition) {
    case 0: return "Stable";
    case 1: return "Describing";
    case 2: return "Preparing";
    case 3: return "Configuring";
    case 4: return "Starting";
    case 5: return "Stopping";
    case 6: return "Pausing";
    case 7: return "Resuming";
    case 8: return "Breaking up";
    case 9: return "Resetting";
    default: return QString("Unknown (%1)").arg(transition);
    }
}
