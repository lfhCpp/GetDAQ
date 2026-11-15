#include "scanplancommands_p.h"
#include "scanexecutionpolicy.h"
#include "scanexecutionsnapshot.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <stdexcept>

using namespace ConfigToolScanPlanCommandsInternal;

int ConfigToolScanPlanCommands::HandleMetadataCommands(const QStringList &args)
{
    if(args.size()==7 && args[1]=="create-scan-policy") {
        try {
            std::string manifest;
            const auto plan=ValidateScanPlanDirectory(args[2],&manifest);
            unsigned int first=0, second=0;
            GetDAQ::ScanExecutionPolicy policy = [&]() {
                if(args[3]=="event-count") {
                    if(!ParseDecimal(args[4],GetDAQ::ScanExecutionPolicy::MaximumEventsPerPoint,&first) ||
                       !ParseDecimal(args[5],GetDAQ::ScanExecutionPolicy::MaximumEventRunDurationMs,&second))
                        throw std::invalid_argument("Event count and maximum duration must be canonical bounded decimal integers");
                    return GetDAQ::ScanExecutionPolicy::ForEventCount(manifest,plan,first,second);
                }
                if(args[3]=="timed-windows")
                    throw std::invalid_argument("Timed-window policies are disabled; Threshold plans are manual-only and cannot be automatically run");
                throw std::invalid_argument("Stopping strategy must be event-count; Threshold automatic Run is disabled");
            }();
            const QFileInfo outputInfo(args[6]);
            const QString output=QDir::cleanPath(outputInfo.absoluteFilePath());
            const QFileInfo parentInfo(QFileInfo(output).absolutePath());
            if(parentInfo.isSymLink() || !parentInfo.isDir() ||
               parentInfo.canonicalFilePath()!=QDir::cleanPath(parentInfo.absoluteFilePath()) ||
               QFileInfo::exists(output) || QFileInfo(output).isSymLink())
                throw std::runtime_error("Policy output parent must be a real directory and output must not already exist");
            QString error;
            const QByteArray encoded=QByteArray::fromStdString(policy.Encode());
            if(!SaveNewMetadataFile(output,encoded,&error,QStringLiteral("Policy"))) throw std::runtime_error(error.toStdString());
            QTextStream(stdout)<<QJsonDocument(QJsonObject{{"created",true},{"policy",output},
                {"source_plan_sha256",QString::fromStdString(policy.SourcePlanSha256())},
                {"stopping_strategy",args[3]},{"hardware_command_sent",false}}).toJson(QJsonDocument::Compact)<<'\n';
            return 0;
        } catch(const std::exception &e) { QTextStream(stderr)<<e.what()<<'\n'; return 1; }
    }

    if(args.size()==6 && args[1]=="create-scan-snapshot") {
        try {
            std::string manifest;
            (void)ValidateScanPlanDirectory(args[2],&manifest);
            const std::string policy=ReadScanPlanFile(args[3],4096);
            const GetDAQ::ScanExecutionSnapshot snapshot=GetDAQ::ScanExecutionSnapshot::Create(
                args[4].toStdString(),manifest,policy,[&](const std::string &name) {
                    return ReadScanPlanFile(QDir(args[2]).filePath(QString::fromLatin1(name.c_str())),
                                            64LL*1024*1024);
                });
            const QFileInfo outputInfo(args[5]);
            const QString output=QDir::cleanPath(outputInfo.absoluteFilePath());
            const QFileInfo parentInfo(QFileInfo(output).absolutePath());
            if(parentInfo.isSymLink() || !parentInfo.isDir() ||
               parentInfo.canonicalFilePath()!=QDir::cleanPath(parentInfo.absoluteFilePath()) ||
               QFileInfo::exists(output) || QFileInfo(output).isSymLink())
                throw std::runtime_error("Execution snapshot output parent must be a real directory and output must not already exist");
            QString error;
            if(!SaveNewMetadataFile(output,QByteArray::fromStdString(snapshot.Encode()),&error,
                                    QStringLiteral("Execution snapshot")))
                throw std::runtime_error(error.toStdString());
            QTextStream(stdout)<<QJsonDocument(QJsonObject{{"created",true},{"snapshot",output},
                {"execution_id",QString::fromStdString(snapshot.ExecutionId())},
                {"source_plan_sha256",QString::fromStdString(snapshot.SourcePlanSha256())},
                {"source_policy_sha256",QString::fromStdString(snapshot.SourcePolicySha256())},
                {"point_count",static_cast<int>(snapshot.PointCount())},
                {"hardware_command_sent",false}}).toJson(QJsonDocument::Compact)<<'\n';
            return 0;
        } catch(const std::exception &e) { QTextStream(stderr)<<e.what()<<'\n'; return 1; }
    }

    return -1;
}
