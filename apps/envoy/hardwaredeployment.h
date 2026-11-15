#ifndef GETDAQ_HARDWAREDEPLOYMENT_H
#define GETDAQ_HARDWAREDEPLOYMENT_H
#include "runconfig.h"
#include "systemexperimentconfiguration.h"
#include <QJsonObject>

// Local, fresh deployments only. Existing service state is never rewritten.
struct HardwareBoardSetup {
    QString templateId; // Reviewed electronics settings copied from this board.
    QString id, address, descriptionFile, nodeObject, nodeName, linkObject, alignmentObject, senderObject;
    QString receiverAddress, mutantRole;
    QString receiverNodeId; // Distributed draft only; local deployments ignore it.
    int controlPort=46000, senderPort=46000, dataPort=23001, statusPort=23002;
    bool mutant=false, enabled=true, receivesData=true;
};
struct HardwareDeploymentSetup {
    QString experimentFile, profileFile, programDirectory, deploymentDirectory, dataDirectory, callbackAddress;
    bool acceptLegacyDelayReadback=false;
    int eccPort=24000, supervisorPort=23100, callbackPort=24100;
    QVector<HardwareBoardSetup> boards;
};
struct HardwareDeploymentFiles {
    QString directory, eccFile, supervisorFile, receiverExecutable;
    QJsonObject ecc;
    RunConfig console;
};
HardwareDeploymentSetup ReadHardwareDeploymentSetup(const QString &directory);
GetDAQ::SystemExperimentArtifact CompileHardwareSetupExperiment(const QString &experiment,const QString &profile);
QVector<HardwareBoardSetup> HardwareSetupBoards(const GetDAQ::SystemExperimentArtifact &artifact);
HardwareDeploymentFiles WriteHardwareDeployment(const HardwareDeploymentSetup &setup,const RunConfig &console);
QByteArray ReadHardwareSetupFile(const QString &path,qint64 maximum=64*1024*1024);
void WriteHardwareSetupFile(const QString &path,const QByteArray &bytes);
QString HardwareSetupSha256(const QByteArray &bytes);
#endif
