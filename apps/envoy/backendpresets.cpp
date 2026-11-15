#include "backendpresets.h"
#include "hardwarecompiler.h"
#include "xcfgdocument.h"
#include <QJsonDocument>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QNetworkInterface>
#include <QStandardPaths>
#include <QHostAddress>
#include <stdexcept>
HardwareDeploymentSetup BuiltinPedestalsBackendSetup() {
    HardwareDeploymentSetup setup;setup.programDirectory=QCoreApplication::applicationDirPath();
    QByteArray combined;
    const QStringList names={"pedestals-single-cobo.xcfg","pedestals-profile.json","cobo-hardware.xcfg"};
    for(const auto &name:names)combined+=ReadHardwareSetupFile(":/backend-presets/"+name);
    const QString root=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/presets/"+HardwareSetupSha256(combined);
    if(!QDir().mkpath(root) || !QFile::setPermissions(root,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner))throw std::runtime_error("Cannot create built-in preset directory");
    for(const auto &name:names) {
        const auto bytes=ReadHardwareSetupFile(":/backend-presets/"+name);const auto file=root+'/'+name;
        if(!QFile::exists(file))WriteHardwareSetupFile(file,bytes);
        if(ReadHardwareSetupFile(file)!=bytes)throw std::runtime_error("Built-in preset file changed; select an explicit custom template instead");
    }
    setup.experimentFile=root+"/pedestals-single-cobo.xcfg";setup.profileFile=root+"/pedestals-profile.json";
    XcfgDocument description;QJsonObject hardware;QString error;
    if(!description.Load(root+"/cobo-hardware.xcfg",&error) || !HardwareCompiler::Compile(description,&hardware,&error))throw std::runtime_error(error.toStdString());
    hardware["node_id"]="CoBo[0]";
    const auto hardwareBytes=QJsonDocument(hardware).toJson();const auto hardwareFile=root+"/cobo-hardware.json";
    if(!QFile::exists(hardwareFile))WriteHardwareSetupFile(hardwareFile,hardwareBytes);
    if(ReadHardwareSetupFile(hardwareFile)!=hardwareBytes)throw std::runtime_error("Built-in hardware description changed");
    setup.boards=HardwareSetupBoards(CompileHardwareSetupExperiment(setup.experimentFile,setup.profileFile));
    QString local="127.0.0.1";
    for(const auto &address:QNetworkInterface::allAddresses())if(address.protocol()==QAbstractSocket::IPv4Protocol && !address.isLoopback()){local=address.toString();break;}
    setup.callbackAddress=qEnvironmentVariable("GETDAQ_CONTROL_IPV4",local);
    for(auto &board:setup.boards){board.nodeObject="HwNode";board.nodeName="CoBo[0]";board.controlPort=46001;board.senderPort=46004;board.address=qEnvironmentVariable("GETDAQ_COBO_IPV4","192.168.40.64");board.receiverAddress=qEnvironmentVariable("GETDAQ_RECEIVER_IPV4",local);board.descriptionFile=hardwareFile;}
    return setup;
}
