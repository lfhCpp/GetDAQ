#include "scanplancommands_p.h"
#include "systemexperimentcompiler.h"
#include "thresholdscan.h"
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

int HandleThresholdScanPlanCommand(const QStringList &args) {
    if (args.size() != 10 || args[1] != "plan-threshold-scan") return -1;

        try {
            unsigned int asadId=0, agetId=0, start=0, stop=0, step=0;
            if (!ParseDecimal(args[4],3,&asadId) || !ParseDecimal(args[5],3,&agetId) ||
                !ParseDecimal(args[6],127,&start) || !ParseDecimal(args[7],127,&stop) ||
                !ParseDecimal(args[8],127,&step))
                throw std::runtime_error("AsAd/AGET IDs must be decimal 0..3; threshold codes must be decimal 0..127");
            const QFileInfo target(args[9]);
            const QString targetPath=target.absoluteFilePath();
            ValidateGeneratedScanPlanTarget(targetPath);
            const GetDAQ::SystemExperimentArtifact baseline=GetDAQ::LoadSystemExperimentArtifact(args[2].toStdString());
            const auto points=GetDAQ::BuildThresholdScan(baseline,args[3].toStdString(),asadId,agetId,start,stop,step);
            PublishGeneratedScanPlan(targetPath,QStringLiteral("threshold-scan"),[&](const QString &stagingPath) {
                QJsonArray entries;
                for (const auto &point:points) {
                    const QString filename=QStringLiteral("point-%1.json").arg(point.index,3,10,QChar('0'));
                    const std::string encoded=GetDAQ::EncodeSystemExperimentArtifact(point.artifact);
                    QString error;
                    if (!SavePlanFile(QDir(stagingPath).filePath(filename),QByteArray(encoded.data(),int(encoded.size())),&error))
                        throw std::runtime_error(error.toStdString());
                    entries.append(QJsonObject{{"index",int(point.index)}, {"threshold_code",int(point.thresholdCode)},
                        {"artifact",filename}, {"configuration_sha256",QString::fromStdString(point.artifact.configurationSha256)},
                        {"artifact_sha256",QString::fromStdString(point.artifact.artifactSha256)}});
                }
                return QJsonObject{{"schema_version",1},{"kind","getdaq.aget-threshold-scan"},
                    {"target_cobo",args[3]}, {"target_asad",int(asadId)}, {"target_aget",int(agetId)},
                    {"start_code",int(start)}, {"stop_code",int(stop)}, {"step_code",int(step)},
                    {"hardware_loaded_verified",false}, {"requires_full_configure_per_point",true},
                    {"acquisition_event_count_unspecified",true}, {"points",entries}};
            });
            QTextStream(stdout)<<"Created offline "<<points.size()<<"-point AGET threshold plan: "<<targetPath
                               <<"\nNo hardware commands or data collection were performed; review each point before full Configure/acquisition Run.\n";
            return 0;
        } catch (const std::exception &e) { QTextStream(stderr)<<"Cannot create AGET threshold scan plan: "<<e.what()<<'\n'; return 1; }
    
}

}
