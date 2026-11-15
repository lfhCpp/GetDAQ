#include "eccservice.h"
#include <stdexcept>
#include "eccsuccessorplan.h"
#include "systemexperimentconfiguration.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QFileInfo>
#include <QTimer>
#include <csignal>
#include <iostream>
namespace { volatile std::sig_atomic_t interrupted=0; void Stop(int) { interrupted=1; } }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); app.setApplicationName("getdaq-ecc-server"); app.setApplicationVersion("1.0");
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption(); parser.setApplicationDescription("Fixed native ECC control service; raw data stays in independent receiver processes.");
    parser.addOption({"config","Fixed ECC service input JSON.","path"}); parser.addOption({"check-config","Validate frozen inputs without creating ECC/receiver roots or sending remote commands."});
    parser.addOption({"plan-config","Compile the exact next ECC snapshot/control scope without hardware, audit roots or host reservations."});
    parser.addOption({"create-successor-plan","Exclusively save a sealed successor identity; does not activate or transfer control.","path"});
    parser.addOption({"check-successor-plan","Check a sealed successor identity against exact fixed inputs; does not activate it.","path"});
    parser.addOption({"successor-plan","Start once with this frozen successor identity; requires --handoff-receipt.","path"});
    parser.addOption({"handoff-receipt","Original complete data-control handoff receipt for planned startup.","path"});
    parser.addOption({"generate-token","Create exclusive 0600 credential file; never print its value.","path"}); parser.process(app);
    try {
        std::signal(SIGPIPE,SIG_IGN); std::signal(SIGINT,Stop); std::signal(SIGTERM,Stop);
        if (parser.isSet("generate-token")) {
            if (parser.isSet("config") || parser.isSet("check-config") || parser.isSet("plan-config") ||
                parser.isSet("create-successor-plan") || parser.isSet("check-successor-plan") ||
                parser.isSet("successor-plan") || parser.isSet("handoff-receipt")) throw std::invalid_argument("Credential provisioning is separate from ECC startup");
            const auto path=parser.value("generate-token").toStdString(); GetDAQ::SupervisorIO::Path(path); GetDAQ::SupervisorIO::WriteNew(path,GetDAQ::SupervisorIO::RandomSession()+"\n"); qInfo()<<"Credential file created; value not displayed."; return 0;
        }
        const unsigned int offline=unsigned(parser.isSet("check-config"))+unsigned(parser.isSet("plan-config"))+
            unsigned(parser.isSet("create-successor-plan"))+unsigned(parser.isSet("check-successor-plan"));
        if (offline>1 || (offline && (parser.isSet("successor-plan") || parser.isSet("handoff-receipt"))))
            throw std::invalid_argument("Choose one offline action or startup with a successor plan/receipt pair");
        if (parser.isSet("successor-plan")!=parser.isSet("handoff-receipt"))
            throw std::invalid_argument("Planned startup requires both --successor-plan and --handoff-receipt");
        if (!parser.isSet("config")) throw std::invalid_argument("Supply --config with fixed ECC service JSON");
        const auto file=QFileInfo(parser.value("config")).absoluteFilePath().toStdString();
        if (parser.isSet("create-successor-plan") || parser.isSet("check-successor-plan")) {
            GetDAQ::FixedEccConfiguration configuration(file);
            if (parser.isSet("create-successor-plan"))
                std::cout<<GetDAQ::EccSuccessorPlan::Create(parser.value("create-successor-plan").toStdString(),configuration);
            else { GetDAQ::EccSuccessorPlan plan(parser.value("check-successor-plan").toStdString(),configuration);std::cout<<plan.Json(); }
            return 0;
        }
        if (parser.isSet("check-config")) { GetDAQ::FixedEccConfiguration configuration(file); configuration.Check(); qInfo().noquote()<<QString("Fixed input validation passed; no ECC/receiver roots created; binding_sha256=%1").arg(QString::fromStdString(configuration.BindingSha256())); return 0; }
        if (parser.isSet("plan-config")) {
            GetDAQ::FixedEccConfiguration configuration(file);configuration.Check();
            const auto options=configuration.ControllerOptions();
            GetDAQ::RemoteReceiverManager data(configuration.ReceiverOptions());
            const auto plan=GetDAQ::PlanNativeEccConfiguration(options,data.BindingSha256());
            const auto artifact=GetDAQ::DecodeSystemExperimentArtifact(options.experimentArtifactBytes);
            configuration.Check();
            std::cout<<"{\"schema\":1,\"kind\":\"getdaq.native-ecc-plan\",\"service_id\":"<<GetDAQ::EncodeJsonString(configuration.ServiceId())
                <<",\"input_binding_sha256\":"<<GetDAQ::EncodeJsonString(configuration.BindingSha256())
                <<",\"snapshot_sha256\":"<<GetDAQ::EncodeJsonString(plan.snapshotSha256)
                <<",\"data_controller_scope_sha256\":"<<GetDAQ::EncodeJsonString(plan.dataControllerScopeSha256)
                <<",\"data_lifecycle_binding_sha256\":"<<GetDAQ::EncodeJsonString(data.BindingSha256())
                <<",\"configuration_sha256\":"<<GetDAQ::EncodeJsonString(artifact.configurationSha256)
                <<",\"artifact_sha256\":"<<GetDAQ::EncodeJsonString(artifact.artifactSha256)
                <<",\"audit_root\":"<<GetDAQ::EncodeJsonString(options.auditRoot)
                <<",\"audit_roots_created\":false,\"host_reservations_acquired\":false,\"hardware_commands_sent\":false,\"receiver_started\":false}\n";
            return 0;
        }
        GetDAQ::EccService service(file,parser.value("successor-plan").toStdString(),parser.value("handoff-receipt").toStdString());
        QObject::connect(&service,&GetDAQ::EccService::ShutdownRequestedSignal,&app,&QCoreApplication::quit);
        service.Start(); QTimer shutdown; QObject::connect(&shutdown,&QTimer::timeout,&app,[&] { if (interrupted) app.quit(); }); shutdown.start(100);
        qInfo()<<"getdaq-ecc-server control listener active; port"<<service.Port()<<"; raw data is not forwarded"; return app.exec();
    } catch (const std::exception &e) { qCritical().noquote()<<e.what(); return 2; }
}
