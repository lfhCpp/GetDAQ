#ifndef RUNCONFIG_H
#define RUNCONFIG_H

#include <QMap>
#include <QMetaType>
#include <QString>
#include <QVector>

#include "autorun.h"

struct NodeConfig
{
    int id = 0;
    bool mutant = false;
    QString name;
    QString host;
    int eccPort = 8083;
    int sentryPort = 8080;
    int routerPort = 46005;
    int exporterPort = 0; // 新的单入口接收/导出部署不再让板端建立第二条原始数据链路；显式端口仍兼容旧目标。
    int previewPort = 0; // TCP preview export, separate from board data links.
    QString previewHost;
    QString routerHost;      // 留空表示沿用 ECC 主机；端口 0 表示关闭该数据目标。
    QString exporterHost;
    QString sentryHost;      // 数据盘所在机器的代理，允许与 ECC 主机分开。
    QString describeId;
    QString prepareId;
    QString configureId;
    QString soapNamespace = "urn:http://localhost:8083/";
    bool soapInlineXml = false; // gSOAP 字符串参数默认转义；兼容旧 envoy 的内嵌 XML 可显式开启。
    QString ConfigId(const QString &kind, const QString &experiment) const;
};

// 保留 envoy 的四个 YAML 字段；设备和本地目录放在 getdaq 节点。
struct RunConfig
{
    QString experiment = "Exp";
    int runNumber = 0;
    QString description;
    QString configurationBackupWarning;
    QString configurationSnapshot; // Immutable managed release used for Run backups.
    QString deploymentProject; // Local managed project.json path; contains endpoint references, not tokens.
    bool syncConfiguration=true;
    QString eccName; // Friendly connection name, independent of board IDs and fixed service identity.
    QMap<QString, QString> fields;
    QVector<NodeConfig> nodes;
    bool simulation = false; // Legacy-file marker only; true is rejected, never enables a fake backend.
    bool nativeEcc = false; // Import guard only: true is rejected; no native runtime exists.
    QString configDir;
    QString outputDir;
    QString filePath;
    AutoRunSettings autoRun;
    int pollMs = 2000;
    int timeoutMs = 10000;
    int catalogTimeoutMs = 120000;
    int stepTimeoutMs = 120000;
    int settleMs = 2000;

    static RunConfig Default();
    bool Validate(QString *error) const;
    bool Load(const QString &path, QString *error);
    bool Save(const QString &path, QString *error) const;
    QString RunDirectory() const;
};
Q_DECLARE_METATYPE(RunConfig)

struct NodeStatus
{
    int id = 0;
    int state = 0;
    int transition = 0;
    bool eccOnline = false;
    bool sentryOnline = false;
    QString error;
    QString sentryError;
    QString disk;
    QString process;
    QString dataPath;
    int files = 0;
    bool projectConfigurationPending=false;
    bool sentrySourceSpecific=false;
    int projectFiles=-1;
    quint64 projectFileBytes=0;
    bool projectFileBytesKnown=false;
    double projectRateMB=0;
    bool projectRateKnown=false;
    double writtenGB = 0;
    quint64 rawFileBytes=0;
    bool rawFileBytesKnown=false;
    double availableGB = 0;
    double totalGB = 0;
    double rateMB = 0;
    bool sentryRateKnown = false;
    bool sentryCatalogPending = false;
    qint64 eccTime = -1;
    qint64 sentryTime = -1;

};
Q_DECLARE_METATYPE(NodeStatus)
Q_DECLARE_METATYPE(QVector<NodeStatus>)
QString StateName(int state);
QString TransitionName(int transition);

#endif
