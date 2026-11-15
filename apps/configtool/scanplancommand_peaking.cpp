#include "scanplancommands_p.h"
#include "systemexperimentcompiler.h"
#include "peakingtimescan.h"
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

int HandlePeakingScanPlanCommand(const QStringList &args) {
    if (args.size() != 5 || args[1] != "plan-peaking-scan") return -1;

        try {
            const QFileInfo target(args[4]);
            const QString targetPath=target.absoluteFilePath();
            ValidateGeneratedScanPlanTarget(targetPath);
            const GetDAQ::SystemExperimentArtifact baseline=GetDAQ::LoadSystemExperimentArtifact(args[2].toStdString());
            const auto points=GetDAQ::BuildPeakingTimeScan(baseline,args[3].toStdString());
            PublishGeneratedScanPlan(targetPath,QStringLiteral("peaking-scan"),[&](const QString &stagingPath) {
                QJsonArray entries;
                for (const auto &point:points) {
                    const QString filename=QStringLiteral("point-%1.json").arg(point.index,2,10,QChar('0'));
                    const std::string encoded=GetDAQ::EncodeSystemExperimentArtifact(point.artifact);
                    QString error;
                    if (!SavePlanFile(QDir(stagingPath).filePath(filename),QByteArray(encoded.data(),int(encoded.size())),&error))
                        throw std::runtime_error(error.toStdString());
                    entries.append(QJsonObject{{"index",int(point.index)}, {"artifact",filename},
                        {"configuration_sha256",QString::fromStdString(point.artifact.configurationSha256)},
                        {"artifact_sha256",QString::fromStdString(point.artifact.artifactSha256)}});
                }
                return QJsonObject{{"schema_version",1},{"kind","getdaq.peaking-time-scan"},
                    {"target_cobo",args[3]}, {"hardware_loaded_verified",false},
                    {"requires_full_configure_per_point",true}, {"acquisition_event_count_unspecified",true},
                    {"points",entries}};
            });
            QTextStream(stdout)<<"Created offline 16-point peaking-time plan: "<<targetPath
                               <<"\nNo hardware commands were sent; each point requires a separately reviewed full Configure/acquisition Run.\n";
            return 0;
        } catch (const std::exception &e) { QTextStream(stderr)<<"Cannot create peaking-time scan plan: "<<e.what()<<'\n'; return 1; }
    
}

}
