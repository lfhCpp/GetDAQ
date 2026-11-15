#include "scanplancommands_p.h"
#include "systemexperimentcompiler.h"
#include "autogainscan.h"
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QJsonArray>
#include <QJsonObject>
#include <stdexcept>

namespace ConfigToolScanPlanCommandsInternal {

int HandleAutoGainScanPlanCommand(const QStringList &args) {
    if (args.size() != 12 || args[1] != "plan-autogain-scan") return -1;

        try {
            unsigned int asadId=0, agetId=0, firstChannel=0, lastChannel=0;
            bool startOk=false, stopOk=false, stepOk=false;
            const int start=args[8].toInt(&startOk,10), stop=args[9].toInt(&stopOk,10), step=args[10].toInt(&stepOk,10);
            if (!ParseDecimal(args[4],3,&asadId) || !ParseDecimal(args[5],3,&agetId) ||
                !ParseDecimal(args[6],67,&firstChannel) || !ParseDecimal(args[7],67,&lastChannel) ||
                !startOk || !stopOk || !stepOk)
                throw std::runtime_error("Chip/channel IDs must be canonical decimal; voltage range must be integer mV");
            const QFileInfo target(args[11]);
            const QString targetPath=target.absoluteFilePath();
            ValidateGeneratedScanPlanTarget(targetPath);
            const GetDAQ::SystemExperimentArtifact baseline=GetDAQ::LoadSystemExperimentArtifact(args[2].toStdString());
            const auto points=GetDAQ::BuildAutoGainScan(baseline,args[3].toStdString(),asadId,agetId,
                firstChannel,lastChannel,start,stop,step);
            PublishGeneratedScanPlan(targetPath,QStringLiteral("autogain-scan"),[&](const QString &stagingPath) {
                QJsonArray entries;
                for (const auto &point:points) {
                    const QString filename=QStringLiteral("point-%1.json").arg(point.index,3,10,QChar('0'));
                    const std::string encoded=GetDAQ::EncodeSystemExperimentArtifact(point.artifact);
                    QString error;
                    if (!SavePlanFile(QDir(stagingPath).filePath(filename),QByteArray(encoded.data(),int(encoded.size())),&error))
                        throw std::runtime_error(error.toStdString());
                    entries.append(QJsonObject{{"index",int(point.index)}, {"channel_id",int(point.channelId)},
                        {"voltage_mv",point.voltageMv}, {"artifact",filename},
                        {"configuration_sha256",QString::fromStdString(point.artifact.configurationSha256)},
                        {"artifact_sha256",QString::fromStdString(point.artifact.artifactSha256)}});
                }
                return QJsonObject{{"schema_version",1},{"kind","getdaq.auto-gain-scan"},
                    {"target_cobo",args[3]}, {"target_asad",int(asadId)}, {"target_aget",int(agetId)},
                    {"first_channel",int(firstChannel)}, {"last_channel",int(lastChannel)},
                    {"start_mv",start}, {"stop_mv",stop}, {"step_mv",step},
                    {"channel_order","aget-reg3-bit-ascending-then-reg4-bit-ascending"},
                    {"legacy_channel_mask_mapping_verified_from_source",true},
                    {"hardware_loaded_verified",false}, {"requires_full_configure_per_point",true},
                    {"acquisition_event_count_unspecified",true}, {"gain_result_interpretation_verified",false},
                    {"points",entries}};
            });
            QTextStream(stdout)<<"Created offline "<<points.size()<<"-point AutoGain configuration plan: "<<targetPath
                <<"\nChannel-selection register mapping is source-derived, but no gain judgment, hardware command or data acquisition is included.\n";
            return 0;
        } catch (const std::exception &e) { QTextStream(stderr)<<"Cannot create AutoGain scan plan: "<<e.what()<<'\n'; return 1; }
    
}

}
