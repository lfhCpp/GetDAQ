#include "hardwaredeployment.h"
#include "systemexperimentcompiler.h"
#include "hardwarecompiler.h"
#include "hardwaredescription.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <stdexcept>

namespace {
void Require(bool ok,const QString &message) { if(!ok)throw std::runtime_error(message.toStdString()); }
QString S(const std::string &s) {return QString::fromStdString(s);}
void Address(const QString &value,bool local=false) {
    QHostAddress ip(value);
    Require(ip.protocol()==QAbstractSocket::IPv4Protocol && ip.toString()==value &&
        ip!=QHostAddress::AnyIPv4 && !ip.isMulticast() && ip!=QHostAddress::Broadcast,
        QStringLiteral("Enter a concrete IPv4 address: %1").arg(value));
    if(local)Require(QNetworkInterface::allAddresses().contains(ip),QStringLiteral("Address is not on this computer: %1").arg(value));
}
void Port(int value) {Require(value>0 && value<=65535,QStringLiteral("Ports must be 1..65535"));}
void Object(const QString &value) {
    Require(QRegularExpression(QStringLiteral("^[A-Za-z0-9_./-]{1,128}$")).match(value).hasMatch() &&
        !value.contains(QStringLiteral("REPLACE")),QStringLiteral("Enter the actual board object/node name: %1").arg(value));
}
void NodeName(const QString &value) {
    Require(QRegularExpression(QStringLiteral(R"(^[A-Za-z0-9_./\[\]-]{1,128}$)")).match(value).hasMatch() && !value.contains(QStringLiteral("REPLACE")),QStringLiteral("Enter the actual board node name: %1").arg(value));
}
QString Proxy(const QString &object,const HardwareBoardSetup &board,int port) {
    Object(object);Port(port);Address(board.address);
    return QStringLiteral("%1:tcp -h %2 -p %3").arg(object,board.address).arg(port);
}
void PrivateDirectory(const QString &path,bool fresh=false) {
    QFileInfo info(path);
    Require(QDir::isAbsolutePath(path) && QDir::cleanPath(path)==path && !info.isSymLink(),QStringLiteral("Use an absolute path without symlinks: %1").arg(path));
    if(info.exists()) {
        Require(!fresh && info.isDir() && info.ownerId()==QFileInfo(QDir::homePath()).ownerId() &&
            !(info.permissions() & (QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther)),
            QStringLiteral("Directory must be new or privately owned (0700): %1").arg(path));
    } else {
        Require(QDir().mkpath(path) && QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),QStringLiteral("Cannot create private directory: %1").arg(path));
    }
    Require(QFileInfo(path).canonicalFilePath()==path,QStringLiteral("Directory path crosses a symlink: %1").arg(path));
}
QByteArray Token() {
    QByteArray bytes;
    for(int i=0;i<8;++i)bytes+=QByteArray::number(QRandomGenerator::system()->generate(),16).rightJustified(8,'0');
    return bytes+'\n';
}
QJsonObject Ref(const QString &path) {return {{"path",path},{"sha256",HardwareSetupSha256(ReadHardwareSetupFile(path))}};}
QByteArray Json(const QJsonObject &object) {return QJsonDocument(object).toJson();}
}

QByteArray ReadHardwareSetupFile(const QString &path,qint64 maximum) {
    QFile file(path);
    Require(!QFileInfo(path).isSymLink() && file.open(QIODevice::ReadOnly) && file.size()<=maximum,
        QStringLiteral("Cannot read bounded setup file: %1").arg(path));
    const auto bytes=file.read(maximum+1);
    Require(bytes.size()<=maximum && file.error()==QFile::NoError,QStringLiteral("Cannot read setup file: %1").arg(path));return bytes;
}
void WriteHardwareSetupFile(const QString &path,const QByteArray &bytes) {
    QFile file(path);
    Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),QStringLiteral("Will not overwrite setup file: %1").arg(path));
    Require(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(bytes)==bytes.size() && file.flush(),QStringLiteral("Cannot save private setup file: %1").arg(path));
}
QString HardwareSetupSha256(const QByteArray &bytes) {return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
GetDAQ::SystemExperimentArtifact CompileHardwareSetupExperiment(const QString &experiment,const QString &profile) {
    if(experiment.endsWith(".json",Qt::CaseInsensitive))
        return GetDAQ::DecodeSystemExperimentArtifact(ReadHardwareSetupFile(experiment).toStdString());
    XcfgDocument document;QString error;GetDAQ::SystemExperimentArtifact artifact;
    Require(document.Load(experiment,&error),error);
    Require(SystemExperimentCompiler::Compile(document,ReadHardwareSetupFile(profile,1024*1024),&artifact,&error),error);
    Require(document.SourcesUnchanged(&error),error);return artifact;
}
QVector<HardwareBoardSetup> HardwareSetupBoards(const GetDAQ::SystemExperimentArtifact &artifact) {
    QVector<HardwareBoardSetup> result;
    for(const auto &c:artifact.settings.cobos) {
        HardwareBoardSetup b;b.id=S(c.id);b.templateId=b.id;b.enabled=c.enabled;b.receivesData=c.enabled;b.nodeName=b.id;
        b.nodeObject=QStringLiteral("REPLACE_COBO_NODE_OBJECT");b.senderObject=QStringLiteral("DaqCtrlNode");b.linkObject=QStringLiteral("MutantLinkManager");
        b.dataPort=23001+result.size()*2;b.statusPort=b.dataPort+1;result.append(b);
    }
    for(const auto &m:artifact.settings.mutants) {
        HardwareBoardSetup b;b.id=S(m.id);b.templateId=b.id;b.mutant=true;b.receivesData=m.settings.readoutEnabled;b.nodeName=b.id;
        b.senderObject=QStringLiteral("DaqCtrlNode");b.dataPort=23001+result.size()*2;b.statusPort=b.dataPort+1;
        b.nodeObject=QStringLiteral("REPLACE_MUTANT_NODE_OBJECT");b.alignmentObject=QStringLiteral("MutantAlignmentServer");
        b.mutantRole=m.settings.isSlave ? QStringLiteral("slave") : QStringLiteral("master");result.append(b);
    }
    return result;
}
HardwareDeploymentFiles WriteHardwareDeployment(const HardwareDeploymentSetup &setup,const RunConfig &base) {
#ifndef Q_OS_LINUX
    Q_UNUSED(setup);Q_UNUSED(base);
    throw std::runtime_error("Automatic service deployment requires GetDAQ running on the Linux service computer.");
#else
    auto artifact=CompileHardwareSetupExperiment(setup.experimentFile,setup.profileFile);
    const auto templateBytes=QByteArray::fromStdString(GetDAQ::EncodeSystemExperimentArtifact(artifact));
    const auto original=artifact.settings;
    artifact.settings.cobos.clear();artifact.settings.mutants.clear();
    QSet<QString> ids;
    for(const auto &b:setup.boards) {
        Object(b.id);Require(!ids.contains(b.id),QStringLiteral("Duplicate board ID: %1").arg(b.id));ids.insert(b.id);
        const auto source=(b.templateId.isEmpty()?b.id:b.templateId).toStdString();bool found=false;
        if(b.mutant)for(auto value:original.mutants)if(value.id==source) {
            Require(b.receivesData==value.settings.readoutEnabled && b.mutantRole==(value.settings.isSlave?"slave":"master"),"MuTaNT role/readout differs from its template");
            value.id=b.id.toStdString();artifact.settings.mutants.push_back(value);found=true;break;
        }
        if(!b.mutant)for(auto value:original.cobos)if(value.id==source) {
            Require(b.receivesData==b.enabled,"CoBo receiver must match its enabled state");
            value.id=b.id.toStdString();value.enabled=b.enabled;artifact.settings.cobos.push_back(value);found=true;break;
        }
        Require(found,QStringLiteral("No reviewed electronics template for %1 (source %2)").arg(b.id,QString::fromStdString(source)));
    }
    GetDAQ::ValidateSystemExperimentSettings(artifact.settings);
    artifact.configurationSha256=GetDAQ::SystemExperimentDigest(artifact.settings);
    Address(setup.callbackAddress,true);Port(setup.eccPort);Port(setup.supervisorPort);Port(setup.callbackPort);
    QSet<int> ports{setup.eccPort,setup.supervisorPort,setup.callbackPort};
    Require(ports.size()==3,QStringLiteral("ECC, supervisor and callback ports must be different"));
    QMap<QString,unsigned int> masks;
    for(const auto &c:artifact.settings.cobos)if(c.enabled)masks.insert(S(c.id),GetDAQ::CompileCoBoConfigure(c.configure).readout.aget.asadMask);
    QVector<QByteArray> descriptions;
    for(int i=0;i<setup.boards.size();++i) {
        const auto &b=setup.boards[i];
        NodeName(b.nodeName);Proxy(b.nodeObject,b,b.controlPort);
        if(b.mutant)Proxy(b.alignmentObject,b,b.controlPort);else Proxy(b.linkObject,b,b.controlPort);
        if(b.receivesData) {
            Proxy(b.senderObject,b,b.senderPort);Address(b.receiverAddress,true);Port(b.dataPort);Port(b.statusPort);
            for(int p:{b.dataPort,b.statusPort}) {Require(!ports.contains(p),QStringLiteral("Duplicate service/data/status/callback port"));ports.insert(p);}
        }
        QByteArray bytes;
        if(b.descriptionFile.endsWith(QStringLiteral(".json"),Qt::CaseInsensitive))bytes=ReadHardwareSetupFile(b.descriptionFile,8*1024*1024);
        else {
            XcfgDocument doc;QJsonObject object;QString error;
            Require(doc.Load(b.descriptionFile,&error),error);Require(HardwareCompiler::Compile(doc,&object,&error),error);
            Require(doc.SourcesUnchanged(&error),error);bytes=Json(object);
        }
        GetDAQ::DecodeHardwareDescription(bytes.toStdString());descriptions.append(bytes);
    }
    const QDir programs(setup.programDirectory);
    for(const QString &name:{QStringLiteral("getdaq-ecc-server"),QStringLiteral("getdaq-supervisor"),QStringLiteral("getdaq-receiver"),QStringLiteral("getdaq-supervisor-query")}) {
        QFileInfo binary(programs.filePath(name));
        Require(binary.isFile() && !binary.isSymLink() && binary.isExecutable() && !(binary.permissions()&(QFile::WriteGroup|QFile::WriteOther)),QStringLiteral("Select the installed bin directory; missing or writable executable: %1").arg(binary.filePath()));
    }
    HardwareDeploymentFiles files;files.directory=setup.deploymentDirectory;
    PrivateDirectory(files.directory,true);
    const QDir root(files.directory);
    WriteHardwareSetupFile(root.filePath("BUILD_INCOMPLETE"),"Setup generation not yet validated\n");
    PrivateDirectory(root.filePath("credentials"));PrivateDirectory(root.filePath("state"));PrivateDirectory(root.filePath("logs"));
    PrivateDirectory(setup.dataDirectory);
    const QString eccToken=root.filePath("credentials/ecc.token"),nodeToken=root.filePath("credentials/node.token");
    WriteHardwareSetupFile(eccToken,Token());WriteHardwareSetupFile(nodeToken,Token());
    const QString experiment=root.filePath("experiment.json"),topologyFile=root.filePath("topology.json");
    QJsonArray cobos,mutants,policies,senders,sources,refs;
    // Persist the editable input separately from generated service state.
    WriteHardwareSetupFile(root.filePath("template-experiment.json"),templateBytes);
    QJsonArray editableBoards;
    for(const auto &b:setup.boards)editableBoards.append(QJsonObject{
        {"id",b.id},{"template_id",b.templateId.isEmpty()?b.id:b.templateId},{"address",b.address},
        {"description_file",b.descriptionFile},{"node_object",b.nodeObject},{"node_name",b.nodeName},
        {"link_object",b.linkObject},{"alignment_object",b.alignmentObject},{"sender_object",b.senderObject},
        {"receiver_address",b.receiverAddress},{"mutant_role",b.mutantRole},{"control_port",b.controlPort},
        {"sender_port",b.senderPort},{"data_port",b.dataPort},{"status_port",b.statusPort},
        {"mutant",b.mutant},{"enabled",b.enabled},{"receives_data",b.receivesData}});
    WriteHardwareSetupFile(root.filePath("setup-inputs.json"),Json({{"schema",1},{"kind","getdaq.editable-hardware-setup"},
        {"experiment_file",root.filePath("template-experiment.json")},{"profile_file",QString()},{"program_directory",setup.programDirectory},
        {"data_directory",setup.dataDirectory},{"callback_address",setup.callbackAddress},{"ecc_port",setup.eccPort},
        {"supervisor_port",setup.supervisorPort},{"callback_port",setup.callbackPort},{"accept_legacy_delay_readback",setup.acceptLegacyDelayReadback},{"boards",editableBoards}}));
    artifact.sources.push_back({root.filePath("setup-inputs.json").toStdString(),
        HardwareSetupSha256(ReadHardwareSetupFile(root.filePath("setup-inputs.json"))).toStdString()});
    const auto artifactBytes=QByteArray::fromStdString(GetDAQ::EncodeSystemExperimentArtifact(artifact));
    WriteHardwareSetupFile(experiment,artifactBytes);
    if(setup.acceptLegacyDelayReadback) {
        QJsonArray targets;for(const auto &b:setup.boards)if(!b.mutant)targets.append(QJsonObject{{"node_proxy",Proxy(b.nodeObject,b,b.controlPort)},{"node_name",b.nodeName}});
        WriteHardwareSetupFile(root.filePath("readback-warning-policy.json"),Json({{"schema",1},{"kind","getdaq.read-data-delay-warning"},{"accepted",true},{"accepted_readback","255"},{"targets",targets}}));
    }
    RunConfig console=base;console.nodes.clear();
    for(int i=0;i<setup.boards.size();++i) {
        const auto &b=setup.boards[i];const QString desc=root.filePath(QStringLiteral("hardware-%1.json").arg(i));
        WriteHardwareSetupFile(desc,descriptions[i]);refs.append(Ref(desc));
        QJsonObject node{{"id",b.id},{"expected_node_name",b.nodeName},{"node_proxy",Proxy(b.nodeObject,b,b.controlPort)},{"hardware_description_file",desc}};
        NodeConfig display;display.id=i;display.name=b.id;display.mutant=b.mutant;display.host=b.address;display.eccPort=b.controlPort;display.routerHost=b.receiverAddress;display.routerPort=b.dataPort;
        if(b.mutant) {node["role"]=b.mutantRole;node["alignment_proxy"]=Proxy(b.alignmentObject,b,b.controlPort);mutants.append(node);}
        else {
        node["enabled"]=b.enabled;node["link_proxy"]=Proxy(b.linkObject,b,b.controlPort);cobos.append(node);
        policies.append(QJsonObject{{"id",b.id},{"callback_endpoints",QStringLiteral("tcp -h %1 -p %2").arg(setup.callbackAddress).arg(setup.callbackPort)},{"published_endpoints",""},{"max_pending_alarms",1024},{"max_alarm_audit_bytes",16777216}});
        // Each CoBo needs its own callback port.
        auto policy=policies.last().toObject();
        const int callback=setup.callbackPort+i;
        if(i) {Port(callback);Require(!ports.contains(callback),QStringLiteral("CoBo callback port conflicts with another port"));ports.insert(callback);}
        policy["callback_endpoints"]=QStringLiteral("tcp -h %1 -p %2").arg(setup.callbackAddress).arg(callback);policies.replace(policies.size()-1,policy);
        }
        if(!b.receivesData){if(b.mutant)console.nodes.append(display);continue;}
        const QString receiver=QStringLiteral("%1:%2").arg(b.receiverAddress).arg(b.dataPort);
        senders.append(QJsonObject{{"id",b.id},{"control_proxy",Proxy(b.senderObject,b,b.senderPort)},{"receiver_address",receiver},{"circular_buffer_mask",b.mutant?1:int(masks[b.id])},{"always_flush",false},{"exclusive_stopped_owner_confirmed",true},{"encoding_minor",0},{"timeout_ms",3000}});
        sources.append(QJsonObject{{"id",b.id},{"receiver_address",receiver},{"bind",b.receiverAddress},{"output_root",setup.dataDirectory},{"data_port",b.dataPort},{"status_port",b.statusPort},{"export_port",0},{"queue_mib",64},{"max_frame_mib",32},{"rotate_mib",1024},{"export_queue_mib",16},{"min_free_mib",2048}});
        display.nativeSenderId=b.id;display.nativeAgentId="data-node-local";console.nodes.append(display);
    }
    WriteHardwareSetupFile(topologyFile,Json({{"schema_version",1},{"kind","getdaq.alignment-topology"},{"encoding","1.0"},{"timeout_ms",3000},{"cobos",cobos},{"mutants",mutants}}));
    files.receiverExecutable=QFileInfo(programs.filePath("getdaq-receiver")).absoluteFilePath();
    const QString deploymentId=QFileInfo(files.directory).fileName();
    QJsonObject plan{{"schema",1},{"kind","getdaq.data-supervisor-plan"},{"deployment_id",deploymentId},{"audit_root",root.filePath("state/node")},{"executable",files.receiverExecutable},{"executable_sha256",HardwareSetupSha256(ReadHardwareSetupFile(files.receiverExecutable,256*1024*1024))},{"configuration_sha256",S(artifact.configurationSha256)},{"artifact_sha256",HardwareSetupSha256(artifactBytes)},{"startup_timeout_ms",30000},{"finish_timeout_ms",60000},{"status_timeout_ms",1000},{"max_total_queue_mib",sources.size()*64},{"max_total_frame_mib",sources.size()*32},{"sources",sources}};
    files.supervisorFile=root.filePath("supervisor.json");
    WriteHardwareSetupFile(files.supervisorFile,Json({{"schema",1},{"kind","getdaq.supervisor-service"},{"agent_id","data-node-local"},{"controller_id","ecc-local"},{"bind","127.0.0.1"},{"port",setup.supervisorPort},{"credential_file",nodeToken},{"receiver_plan",plan}}));
    QJsonObject endpoint{{"agent_id","data-node-local"},{"plan_sha256",QString(64,'0')},{"address","127.0.0.1"},{"port",setup.supervisorPort},{"credential_file",nodeToken},{"timeout_ms",2000}};
    files.ecc={{"schema",1},{"kind","getdaq.ecc-service"},{"service_id","ecc-local"},{"client_id","console-local"},{"bind","127.0.0.1"},{"port",setup.eccPort},{"credential_file",eccToken},{"audit_root",root.filePath("state/ecc")},{"deployment_id",deploymentId},{"experiment_artifact",Ref(experiment)},{"topology",Ref(topologyFile)},{"hardware_descriptions",refs},{"description_policy","use-existing"},{"current_topology",QJsonValue::Null},{"exclusive_owner_confirmed",true},{"data_processes_stopped",true},{"cobo_policies",policies},{"senders",senders},{"remote_receivers",QJsonObject{{"controller_id","ecc-local"},{"command_timeout_ms",600000},{"poll_interval_ms",100},{"endpoints",QJsonArray{endpoint}}}}};
    files.eccFile=root.filePath("ecc.json");
    console.nativeEcc=true;console.simulation=false;console.nativeCatalog=false;console.nativeClient=EccSystemClientOptions();
    console.nativeClient.address="127.0.0.1";console.nativeClient.port=setup.eccPort;console.nativeClient.serviceId="ecc-local";console.nativeClient.clientId="console-local";console.nativeClient.credentialFile=eccToken;console.nativeClient.clientEpoch=QString::fromLatin1(Token().trimmed());console.nativeClient.receiverStatusEnabled=true;
    console.nativeRecordDir=root.filePath("control-records");console.configDir=QFileInfo(setup.experimentFile).absolutePath();console.outputDir=root.filePath("run-records");console.filePath=root.filePath("envoy.yml");console.stepTimeoutMs=600000;
    files.console=console;return files;
#endif
}

HardwareDeploymentSetup ReadHardwareDeploymentSetup(const QString &directory) {
    const QDir root(directory);HardwareDeploymentSetup setup;
    auto json=[](const QString &file) {
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(ReadHardwareSetupFile(file),&error);
        Require(error.error==QJsonParseError::NoError && doc.isObject(),QStringLiteral("Invalid configuration JSON: %1").arg(file));return doc.object();
    };
    const QString editable=root.filePath("setup-inputs.json");
    if(QFileInfo::exists(editable)) {
        const auto o=json(editable);
        Require(o["kind"]=="getdaq.editable-hardware-setup" && o["schema"].toInt()==1,"Unsupported editable setup format");
        setup.experimentFile=o["experiment_file"].toString();setup.profileFile=o["profile_file"].toString();
        setup.programDirectory=o["program_directory"].toString();setup.dataDirectory=o["data_directory"].toString();
        setup.callbackAddress=o["callback_address"].toString();setup.eccPort=o["ecc_port"].toInt();
        setup.supervisorPort=o["supervisor_port"].toInt();setup.callbackPort=o["callback_port"].toInt();setup.acceptLegacyDelayReadback=o["accept_legacy_delay_readback"].toBool();
        for(const auto &value:o["boards"].toArray()) {
            const auto v=value.toObject();HardwareBoardSetup b;
            b.id=v["id"].toString();b.templateId=v["template_id"].toString();b.address=v["address"].toString();
            b.descriptionFile=v["description_file"].toString();b.nodeObject=v["node_object"].toString();b.nodeName=v["node_name"].toString();
            b.linkObject=v["link_object"].toString();b.alignmentObject=v["alignment_object"].toString();b.senderObject=v["sender_object"].toString();
            b.receiverAddress=v["receiver_address"].toString();b.mutantRole=v["mutant_role"].toString();b.receiverNodeId=v["receiver_node_id"].toString();
            b.controlPort=v["control_port"].toInt();b.senderPort=v["sender_port"].toInt();b.dataPort=v["data_port"].toInt();b.statusPort=v["status_port"].toInt();
            b.mutant=v["mutant"].toBool();b.enabled=v["enabled"].toBool();b.receivesData=v["receives_data"].toBool();setup.boards.append(b);
        }
        return setup;
    }
    // Import actual deployed files, never substitute environment/default IPs.
    const auto ecc=json(root.filePath("ecc.json"));
    setup.experimentFile=ecc["experiment_artifact"].toObject()["path"].toString();
    setup.boards=HardwareSetupBoards(CompileHardwareSetupExperiment(setup.experimentFile,QString()));
    const auto topology=json(ecc["topology"].toObject()["path"].toString());
    const auto node=json(root.filePath("supervisor.json"));const auto plan=node["receiver_plan"].toObject();
    setup.acceptLegacyDelayReadback=QFileInfo::exists(root.filePath("readback-warning-policy.json"));
    setup.eccPort=ecc["port"].toInt();setup.supervisorPort=node["port"].toInt();
    setup.programDirectory=QFileInfo(plan["executable"].toString()).absolutePath();
    auto proxy=[](const QString &text,QString *object,QString *address,int *port) {
        const auto match=QRegularExpression("^([^:]+):tcp -h ([0-9.]+) -p ([0-9]+)$").match(text);
        Require(match.hasMatch(),QStringLiteral("Unsupported proxy; use advanced settings: %1").arg(text));
        *object=match.captured(1);*address=match.captured(2);*port=match.captured(3).toInt();
    };
    for(auto &b:setup.boards) {
        bool found=false;
        for(const auto &value:topology[b.mutant?"mutants":"cobos"].toArray()) {
            const auto v=value.toObject();if(v["id"].toString()!=b.id)continue;
            b.descriptionFile=v["hardware_description_file"].toString();b.nodeName=v["expected_node_name"].toString();
            proxy(v["node_proxy"].toString(),&b.nodeObject,&b.address,&b.controlPort);
            QString ignoredAddress;int ignoredPort;
            proxy(v[b.mutant?"alignment_proxy":"link_proxy"].toString(),b.mutant?&b.alignmentObject:&b.linkObject,&ignoredAddress,&ignoredPort);found=true;break;
        }
        Require(found,QStringLiteral("Board missing from topology: %1").arg(b.id));
        for(const auto &value:ecc["senders"].toArray()) {
            const auto v=value.toObject();if(v["id"].toString()!=b.id)continue;
            QString ignoredAddress;proxy(v["control_proxy"].toString(),&b.senderObject,&ignoredAddress,&b.senderPort);
        }
        for(const auto &value:plan["sources"].toArray()) {
            const auto v=value.toObject();if(v["id"].toString()!=b.id)continue;
            b.receiverAddress=v["bind"].toString();b.dataPort=v["data_port"].toInt();b.statusPort=v["status_port"].toInt();setup.dataDirectory=v["output_root"].toString();
        }
    }
    const auto policies=ecc["cobo_policies"].toArray();
    if(!policies.isEmpty()) {
        const auto m=QRegularExpression("tcp -h ([0-9.]+) -p ([0-9]+)").match(policies[0].toObject()["callback_endpoints"].toString());
        Require(m.hasMatch(),"Cannot read existing CoBo callback endpoint");setup.callbackAddress=m.captured(1);setup.callbackPort=m.captured(2).toInt();
    }
    return setup;
}
