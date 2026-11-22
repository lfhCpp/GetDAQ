#include "runconfig.h"
#include <QDir>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QSet>
#include <limits>
#include <QHostAddress>



QString DefaultWorkspaceDirectory()
{
    const auto selected=qEnvironmentVariable("GETDAQ_WORKSPACE").trimmed();
    return QDir::cleanPath(!selected.isEmpty() && QDir::isAbsolutePath(selected) ? selected : QDir(QDir::homePath()).filePath("GetDAQ"));
}

QString DefaultNodeCacheDirectory()
{
    const auto visible=QDir(DefaultWorkspaceDirectory()).filePath("local-node");
    const auto legacy=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node";
    if(!QFileInfo::exists(visible+"/node.json") && QFileInfo::exists(legacy+"/node.json"))return legacy;
    return visible;
}

QString PreferredNodeCacheDirectory(const QString &saved)
{
    if(saved.isEmpty())return DefaultNodeCacheDirectory();
    const auto visible=QDir(DefaultWorkspaceDirectory()).filePath("local-node");
    const auto legacy=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/local-node";
    if(QDir::cleanPath(saved)==legacy && (!QFileInfo::exists(legacy+"/node.json") || QFileInfo::exists(visible+"/node.json")))return visible;
    return saved;
}

RunConfig RunConfig::Default()
{
    RunConfig c;
    const QString root = DefaultWorkspaceDirectory();
    QDir installation(QCoreApplication::applicationDirPath());installation.cdUp();if(installation.dirName().startsWith("runtime-"))installation.cdUp();
    const auto installedConfig=installation.filePath("config");
    c.configDir=QFileInfo(installedConfig).isDir()?installedConfig:QDir(root).filePath("config");
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

RunConfig RunConfig::NewAcquisition(const QString &experimentName) const
{
    auto config = *this;
    config.filePath.clear();
    config.experiment = experimentName.trimmed();
    config.runNumber = 0;
    config.description.clear();
    config.existingRunPolicy.clear();
    config.configurationBackupWarning.clear();
    config.autoRun = AutoRunSettings();
    for (auto it = config.fields.begin(); it != config.fields.end(); ++it) it.value().clear();
    // Keep endpoints, explicit ECC IDs, deployed release and directories: this is
    // a new acquisition profile using the same services, not a new deployment.
    return config;
}

QString NodeConfig::ConfigId(const QString &kind, const QString &experiment) const
{
    const QString explicitId = kind == "describe" ? describeId : (kind == "prepare" ? prepareId : configureId);
    if (!explicitId.isEmpty()) return explicitId;
    return kind == "describe" && !mutant ? QString("cobo%1").arg(boardIndex>=0?boardIndex:id) : experiment;
}

bool RunConfig::Validate(QString *error) const
{
    QString problem;
    if(nativeEcc){if(error)*error="The rewritten native ECC backend has been removed. Select an original SOAP ECC connection or create an original backend group.";return false;}
    if(simulation) {
        if(error)*error="Simulation mode has been removed. Select or create a real ECC configuration; legacy simulation files are not converted automatically.";
        return false;
    }
    if(!autoRun.Validate(error))return false;

    // 实验名同时用于 GET 配置 ID 和目录，限制路径字符以避免越界。
    if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(experiment).hasMatch())
        problem = "Experiment must contain 1–80 letters, digits, '_' or '-', starting with a letter/digit.";
    else if (!existingRunPolicy.isEmpty() && existingRunPolicy!="overwrite" && existingRunPolicy!="append") problem="Invalid existing Run file policy.";
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
    int mutants = 0;
    for (const NodeConfig &n : nodes) {
        if (n.mutant) ++mutants;
        const QString endpoint = n.host.toLower() + ":" + QString::number(n.eccPort);
        if (n.id < 0 || n.boardIndex < -1 || n.boardIndex>255 || ids.contains(n.id) || (endpoints.contains(endpoint))) problem = "Duplicate/invalid node ID or ECC endpoint.";
        if ((n.host.trimmed().isEmpty() || n.host.contains('/') || n.host.contains(' '))) problem = "Use a hostname or IP address without an URL scheme.";

        if(n.soapNamespace!="urn:ecc" && n.soapNamespace!="urn:http://localhost:8083/") problem="Unsupported ECC SOAP namespace.";
        for (int p : {n.eccPort, n.sentryPort})
            if (p < 1 || p > 65535) problem = "Ports must be in 1–65535.";
        for (int p : {n.routerPort, n.exporterPort, n.previewPort})
            if (p < 0 || p > 65535) problem = "Data ports must be 0 (disabled) or 1–65535.";
        for (const QString &host : {n.routerHost, n.exporterHost, n.sentryHost, n.previewHost})
            if (host.contains('/') || host.contains(QRegularExpression("\\s"))) problem = "Invalid data receiver hostname.";
        ids.insert(n.id);
        endpoints.insert(endpoint);
    }
    if (mutants > 1 || mutants == nodes.size()) problem = "Configure at least one CoBo and a supported MuTaNT count.";

    if (error) *error = problem;
    return problem.isEmpty();
}

QString RunConfig::RunDirectory() const
{
    return QDir(outputDir).filePath(QString("%1/%2/run_%3").arg("hardware", experiment)
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
