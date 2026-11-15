#include "supervisorservice.h"
#include <stdexcept>
#include "receiverplantransition.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>
#include <csignal>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>
namespace { volatile std::sig_atomic_t interrupted=0; void Stop(int) { interrupted=1; } }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv); app.setApplicationName("getdaq-supervisor"); app.setApplicationVersion("1.0");
    QCommandLineParser p; p.addHelpOption(); p.addVersionOption();
    p.setApplicationDescription("Low-frequency receiver control; raw MFM never passes through this Qt service.");
    p.addOption({"config","Fixed receiver plan and control endpoint JSON.","path"});
    p.addOption({"check-config","Validate configuration/credentials without creating control state."});
    p.addOption({"plan-config","Print canonical receiver plan JSON without creating state or sending commands."});
    p.addOption({"write-plan","Exclusively save a sealed canonical plan for original-owner review.","path"});
    p.addOption({"stage-successor-plan","On this receiver host, stage a scan successor plan from a reviewed bundle file.","source-plan"});
    p.addOption({"stage-plan-target","Exclusive destination for --stage-successor-plan.","target-plan"});
    p.addOption({"predecessor-proof","Start once from this original completed plan retirement proof.","path"});
    p.addOption({"generate-token","Create exclusive 0600 credential file; never print the token.","path"}); p.process(app);
    try {
        std::signal(SIGPIPE,SIG_IGN); std::signal(SIGINT,Stop); std::signal(SIGTERM,Stop);
        if (p.isSet("generate-token")) {
            if (p.isSet("config") || p.isSet("check-config") || p.isSet("plan-config") || p.isSet("write-plan") || p.isSet("stage-successor-plan") || p.isSet("stage-plan-target") || p.isSet("predecessor-proof")) throw std::invalid_argument("Token provisioning is separate from service startup");
            const auto path=p.value("generate-token").toStdString(); GetDAQ::SupervisorIO::Path(path); GetDAQ::SupervisorIO::WriteNew(path,GetDAQ::SupervisorIO::RandomSession()+"\n");
            qInfo()<<"Credential file created; value not displayed."; return 0;
        }
        if (!p.isSet("config")) throw std::invalid_argument("Supply --config with fixed service JSON");
        if (p.isSet("stage-successor-plan")!=p.isSet("stage-plan-target")) throw std::invalid_argument("Successor staging requires both --stage-successor-plan and --stage-plan-target");
        const auto offline=unsigned(p.isSet("check-config"))+unsigned(p.isSet("plan-config"))+unsigned(p.isSet("write-plan"))+unsigned(p.isSet("stage-successor-plan"));
        if (offline>1 || ((p.isSet("plan-config") || p.isSet("write-plan") || p.isSet("stage-successor-plan")) && p.isSet("predecessor-proof")))
            throw std::invalid_argument("Choose one offline plan/check action or explicit predecessor startup");
        const auto config=GetDAQ::SupervisorServiceConfig::Load(p.value("config").toStdString());
        if (p.isSet("plan-config") || p.isSet("write-plan")) {
            const auto bytes=GetDAQ::SupervisorIO::EncodeOptions(config.control.receiver,config.control.executableSha256);
            if (p.isSet("plan-config")) std::cout<<bytes;
            else {
                const auto file=p.value("write-plan").toStdString();GetDAQ::SupervisorIO::Path(file);
                GetDAQ::SupervisorIO::Directory parent(file.substr(0,file.find_last_of('/')));
                GetDAQ::SupervisorIO::WriteNew(file,bytes);
                GetDAQ::SupervisorIO::Fd fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
                if (fd.value<0 || ::fchmod(fd.value,0400) || ::fsync(fd.value) || ::fsync(parent.Descriptor())) throw std::runtime_error("Cannot seal canonical data plan");
                parent.Check();
                std::cout<<"{\"schema\":1,\"kind\":\"getdaq.reviewed-data-plan\",\"plan_file\":"<<GetDAQ::EncodeJsonString(file)
                    <<",\"plan_sha256\":"<<GetDAQ::EncodeJsonString(GetDAQ::ConfigurationSha256(bytes))<<"}\n";
            }
            return 0;
        }
        if (p.isSet("stage-successor-plan")) {
            std::cout<<GetDAQ::StageScanSuccessorPlan(p.value("stage-successor-plan").toStdString(),
                p.value("stage-plan-target").toStdString(),config.control.receiver,config.control.executableSha256);
            return 0;
        }
        if (p.isSet("check-config")) {
            if (p.isSet("predecessor-proof")) {GetDAQ::SupervisorPlanPredecessor proof(p.value("predecessor-proof").toStdString(),config.control);proof.Check();}
            qInfo().noquote()<<QString("Configuration/credential validation passed; no control root created. plan_sha256=%1")
                .arg(QString::fromStdString(GetDAQ::ConfigurationSha256(GetDAQ::SupervisorIO::EncodeOptions(config.control.receiver,config.control.executableSha256))));
            return 0;
        }
        GetDAQ::SupervisorService service(config,p.value("predecessor-proof").toStdString()); service.Start();
        QTimer shutdown; QObject::connect(&shutdown,&QTimer::timeout,&app,[&] { if (interrupted) app.quit(); }); shutdown.start(100);
        qInfo().noquote()<<QString("getdaq-supervisor listening %1:%2; agent=%3; raw data remains in independent receiver")
            .arg(QString::fromStdString(config.bindAddress)).arg(service.Port()).arg(QString::fromStdString(config.control.agentId));
        return app.exec();
    } catch (const std::exception &e) { qCritical().noquote()<<e.what(); return 2; }
}
