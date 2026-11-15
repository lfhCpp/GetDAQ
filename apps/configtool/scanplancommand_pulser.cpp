#include "scanplancommands_p.h"
#include "systemexperimentcompiler.h"
#include "pulseramplitudescan.h"
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

int HandlePulserScanPlanCommand(const QStringList &args) {
    if (args.size() != 8 || args[1] != "plan-pulser-scan") return -1;

        try {
            bool startOk=false, stopOk=false, stepOk=false;
            const int start=args[4].toInt(&startOk,10), stop=args[5].toInt(&stopOk,10), step=args[6].toInt(&stepOk,10);
            if (!startOk || !stopOk || !stepOk) throw std::runtime_error("Start, stop and step must be decimal millivolts");
            const QFileInfo target(args[7]);
            const QString targetPath=target.absoluteFilePath();
            ValidateGeneratedScanPlanTarget(targetPath);
            const GetDAQ::SystemExperimentArtifact baseline=GetDAQ::LoadSystemExperimentArtifact(args[2].toStdString());
            const auto points=GetDAQ::BuildPulserAmplitudeScan(baseline,args[3].toStdString(),start,stop,step);
            PublishGeneratedScanPlan(targetPath,QStringLiteral("pulser-scan"),[&](const QString &stagingPath) {
                QJsonArray entries;
                for (const auto &point:points) {
                    const QString filename=QStringLiteral("point-%1.json").arg(point.index,3,10,QChar('0'));
                    const std::string encoded=GetDAQ::EncodeSystemExperimentArtifact(point.artifact);
                    QString error;
                    if (!SavePlanFile(QDir(stagingPath).filePath(filename),QByteArray(encoded.data(),int(encoded.size())),&error))
                        throw std::runtime_error(error.toStdString());
                    entries.append(QJsonObject{{"index",int(point.index)}, {"voltage_mv",point.voltageMv}, {"artifact",filename},
                        {"configuration_sha256",QString::fromStdString(point.artifact.configurationSha256)},
                        {"artifact_sha256",QString::fromStdString(point.artifact.artifactSha256)}});
                }
                return QJsonObject{{"schema_version",1},{"kind","getdaq.pulser-amplitude-scan"},
                    {"target_cobo",args[3]},{"start_mv",start},{"stop_mv",stop},{"step_mv",step},
                    {"hardware_loaded_verified",false},{"requires_full_configure_per_point",true},
                    {"acquisition_event_count_unspecified",true},{"points",entries}};
            });
            QTextStream(stdout)<<"Created offline "<<points.size()<<"-point pulser-amplitude plan: "<<targetPath
                               <<"\nNo hardware commands were sent; review each point before full Configure/acquisition Run.\n";
            return 0;
        } catch (const std::exception &e) { QTextStream(stderr)<<"Cannot create pulser-amplitude scan plan: "<<e.what()<<'\n'; return 1; }
    
}

}
