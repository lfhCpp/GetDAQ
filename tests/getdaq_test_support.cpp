#include "getdaq_test_support_p.h"
#include "mocknode_fixture_p.h"
#include <QCoreApplication>

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSignalSpy>

#include "runcontrol.h"

namespace GetDAQTestSupport {

RunConfig Config(const QString &directory)
{
    RunConfig config = RunConfig::Default();
    config.nodes = {config.nodes.first(), config.nodes.last()};
    config.outputDir = directory;
    config.configDir = directory + "/configs";
    config.pollMs = 60;
    config.timeoutMs = 200;
    config.catalogTimeoutMs = 300;
    config.stepTimeoutMs = 1200;
    config.settleMs = 20;
    AttachMockNodes(config);
    MakeFiles(config);
    return config;
}

void AttachMockNodes(RunConfig &config)
{
    for(auto &node:config.nodes) {
        auto *trace=new QStringList;
        auto *mock=new GetDAQTestFixtures::MockNode(node.name,trace);
        mock->setParent(QCoreApplication::instance());mock->generateData=true;
        QObject::connect(mock,&QObject::destroyed,[trace]{delete trace;});
        if(!mock->isListening())qFatal("Cannot start test-only loopback fixture");
        node.host="127.0.0.1";node.eccPort=mock->serverPort();node.sentryPort=mock->serverPort();
    }
}

bool At(const QSignalSpy &spy, int state)
{
    if (spy.isEmpty()) return false;
    const auto status = qvariant_cast<QVector<NodeStatus>>(spy.last().at(0));
    if (status.isEmpty()) return false;
    for (const auto &node : status)
        if (!node.eccOnline || node.state != state) return false;
    return true;
}

QString Phase(const QSignalSpy &spy)
{
    return spy.isEmpty() ? QString() : spy.last().at(0).toString();
}

void MakeFiles(const RunConfig &config)
{
    QDir().mkpath(config.configDir + "/describe-cobo");
    for (const QString &prefix : {QString("prepare-"), QString("describe-"), QString("configure-")}) {
        QFile file(config.configDir + "/" + prefix + config.experiment + ".xcfg");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Setup><value>1</value></Setup>");
    }
    for (const NodeConfig &node : config.nodes) {
        for (const QString &kind : {QString("describe"), QString("prepare"), QString("configure")}) {
            QFile file(config.configDir + "/" + kind + "-" + node.ConfigId(kind, config.experiment) + ".xcfg");
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("<Setup><value>1</value></Setup>");
        }
    }
}

} // namespace GetDAQTestSupport
