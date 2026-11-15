// Prepare front-end adapted from received CEA GET CoBoConfigParser/CoBoNode.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "systemexperimentcompiler.h"
#include "experimentreader.h"
#include "agetcompiler.h"
#include "clockcompiler.h"
#include "monitoringcompiler.h"
#include "calibrationcompiler.h"
#include "mutantcompiler.h"
#include "../experiment/strictjson.h"
#include "../experiment/configurationsha256.h"
#include <QFile>
#include <set>
#include <stdexcept>
namespace {
using namespace GetDAQ;
void Keys(const JsonValue &v,std::initializer_list<const char *> names) {
    if (v.type!=JsonValue::Object || v.members.size()!=names.size()) throw std::invalid_argument("Unexpected system profile keys");
    for (const char *name:names) v.At(name);
}
std::string Text(const JsonValue &v) { if (v.type!=JsonValue::String) throw std::invalid_argument("System profile requires JSON string"); return v.text; }
bool Bool(const JsonValue &v) { if (v.type!=JsonValue::Boolean) throw std::invalid_argument("System profile requires JSON boolean"); return v.text=="true"; }
unsigned int UInt(const JsonValue &v,unsigned int maximum) {
    const auto text=Text(v); const auto value=ParseCoBoUnsigned(text,"system profile integer",maximum);
    if (std::to_string(value)!=text) throw std::invalid_argument("System profile requires canonical decimal integer string");
    return value;
}
std::map<QString,const JsonValue *> Profiles(const JsonValue &v,std::size_t maximum) {
    if (v.type!=JsonValue::Array || v.items.size()>maximum) throw std::invalid_argument("Invalid system profile node list");
    std::map<QString,const JsonValue *> out;
    for (const auto &p:v.items) if (!out.emplace(QString::fromStdString(Text(p.At("id"))),&p).second) throw std::invalid_argument("Duplicate system profile instance ID");
    return out;
}
QString Id(const QString &path) { return path.mid(path.lastIndexOf('[')+1).chopped(1); }
unsigned int PhysicalId(const QString &path) {
    const auto id=Id(path);
    if (id.size()!=1 || id[0]<'0' || id[0]>'3') throw std::invalid_argument("Prepare requires physical IDs 0..3");
    return id[0].unicode()-'0';
}
void Inputs(ExperimentReader &reader,const QString &path,CoBoPrepareSettings &s) {
    s.manualReadDataDelay=ParseCoBoUnsigned(reader.Scalar(path+"/Module/readDataDelay").toStdString(),"readDataDelay",255);
    for (unsigned int i=0; i<4; ++i) s.foundation.lemo[i]=reader.Scalar(path+QString("/LEMO/L[%1]/function").arg(i),"none").toStdString();
    const auto asads=reader.Children(path,"AsAd");
    if (asads.size()!=4) throw std::invalid_argument("Prepare must declare four AsAd IDs");
    for (const auto &asad:asads) {
        const auto id=PhysicalId(asad);
        if (!reader.Boolean(asad+"/Control/isActive")) continue;
        s.inputs.asadMask|=1U<<id; AsAdInputBoardSettings board; board.asadId=id;
        const auto agets=reader.Children(asad,"Aget");
        if (agets.size()!=4) throw std::invalid_argument("Prepare must declare four AGET physical positions per active AsAd");
        for (const auto &aget:agets) {
            auto &p=board.positions[PhysicalId(aget)]; const auto control=aget+"/Control/";
            p.active=reader.Boolean(control+"isActive");
            // Each default is independent: a missing early field must not silently discard a later explicit setting.
            p.enabled=reader.Boolean(control+"enableAsAdPosition","true");
            p.connectGroundToZAP=reader.Boolean(control+"connectGroundToZAP","false");
            p.antiSparklingEnabled=reader.Boolean(control+"antiSparklingEnabled","true");
        }
        s.inputs.boards.push_back(board);
    }
}
}
bool SystemExperimentCompiler::Compile(const XcfgDocument &document,const QByteArray &profileBytes,SystemExperimentArtifact *output,QString *error) {
    try {
        if (profileBytes.size()>1024*1024) throw std::invalid_argument("System profile exceeds 1 MiB");
        const auto profile=ParseStrictJson(profileBytes.toStdString());
        Keys(profile,{"schema_version","kind","cobos","mutants"});
        if (profile.At("schema_version").type!=JsonValue::Number || profile.At("schema_version").text!="1" || Text(profile.At("kind"))!="getdaq.system-experiment-profile")
            throw std::invalid_argument("Unsupported system experiment profile");
        auto cobos=Profiles(profile.At("cobos"),192), mutants=Profiles(profile.At("mutants"),3);
        if (!document.Sources().contains(document.RootFile())) throw std::invalid_argument("No experiment loaded");
        const auto &root=document.Nodes().at(document.Sources().value(document.RootFile()).root);
        if (root.name!="Setup") throw std::invalid_argument("Expected Setup experiment root");
        ExperimentReader reader(document); SystemExperimentArtifact a; QJsonObject ignored; QString failure;
        a.profileSha256=ConfigurationSha256(profileBytes.toStdString());
        for (const auto &node:reader.Children(root.path,"Node")) {
            if (Id(node)!="CoBo" && Id(node)!="Mutant") throw std::invalid_argument("Unsupported concrete experiment Node: "+node.toStdString());
            for (const auto &instance:reader.Children(node,"Instance")) {
                const auto id=Id(instance);
                auto &profiles=Id(node)=="CoBo" ? cobos : mutants;
                const auto found=profiles.find(id);
                if (found==profiles.end()) throw std::invalid_argument("Missing profile for experiment instance: "+id.toStdString());
                const auto &p=*found->second; profiles.erase(found);
                if (Id(node)=="Mutant") {
                    Keys(p,{"id","register_layout","clock_policy","two_proton_counter_command","allow_prototype_power_df","verify_board_id","expected_board_id"});
                    SystemExperimentMutant m; m.id=id.toStdString(); MutantSettings options;
                    options.moduleCount=static_cast<unsigned int>(profile.At("mutants").items.size());
                    const auto layout=Text(p.At("register_layout")), policy=Text(p.At("clock_policy"));
                    if (layout=="legacy-names") options.layout=MutantSettings::LegacyNamesV15;
                    else if (layout!="modern") throw std::invalid_argument("Invalid MuTaNT register layout");
                    if (policy=="legacy-vhdci") options.clockPolicy=MutantSettings::LegacyVhdciFallback;
                    else if (policy!="strict") throw std::invalid_argument("Invalid MuTaNT clock policy");
                    options.twoProtonCounterCommand=Bool(p.At("two_proton_counter_command")); options.allowPrototypePowerDf=Bool(p.At("allow_prototype_power_df"));
                    options.verifyBoardId=Bool(p.At("verify_board_id")); options.expectedBoardId=UInt(p.At("expected_board_id"),UINT32_MAX);
                    if (!MutantCompiler::Compile(document,id,options,&ignored,&failure,&m.settings)) throw std::runtime_error(failure.toStdString());
                    a.settings.mutants.push_back(std::move(m)); continue;
                }
                SystemExperimentCoBo c; c.id=id.toStdString(); c.enabled=reader.Boolean(instance+"/isActive");
                if (Bool(p.At("enabled"))!=c.enabled) throw std::invalid_argument("Profile enabled flag differs from experiment");
                if (!c.enabled) { Keys(p,{"id","enabled"}); a.settings.cobos.push_back(std::move(c)); continue; }
                Keys(p,{"id","enabled","hardware_version","chip_versions","buffer_layout","verify_hardware_version","firmware_info_registers","interrupt_rate_register",
                    "monitoring_mapping","led_policy","delay_layout","calibration_aget_register","clock_allow_nominal_difference","clock_allow_unverified_upstream","adc_mode"});
                auto &s=c.prepare; s.foundation.hardwareVersion=UInt(p.At("hardware_version"),UINT32_MAX-1);
                s.foundation.verifyHardwareVersion=Bool(p.At("verify_hardware_version")); s.foundation.firmwareInfoRegisters=Bool(p.At("firmware_info_registers"));
                s.foundation.interruptRateRegister=Bool(p.At("interrupt_rate_register"));
                const auto buffer=Text(p.At("buffer_layout"));
                if (buffer=="shared") s.foundation.bufferLayout=CoBoReadoutSettings::SharedBuffer;
                else if (buffer!="per-asad") throw std::invalid_argument("Invalid CoBo buffer layout");
                const auto mapping=Text(p.At("monitoring_mapping")), delay=Text(p.At("delay_layout")), led=Text(p.At("led_policy")), adc=Text(p.At("adc_mode"));
                if ((mapping!="declared" && mapping!="legacy") || (delay!="global" && delay!="per-asad") || (led!="none" && led!="required") || (adc!="DDR" && adc!="SDR"))
                    throw std::invalid_argument("Invalid explicit CoBo compatibility policy");
                const auto &chips=p.At("chip_versions"); if (chips.type!=JsonValue::Object || chips.members.size()>16) throw std::invalid_argument("Invalid expected chip version map");
                QMap<int,quint16> versions;
                for (const auto &entry:chips.members) {
                    JsonValue key; key.type=JsonValue::String; key.text=entry.first; const auto index=UInt(key,15); const auto version=Text(entry.second);
                    if (version!="0x201" && version!="0x202" && version!="0x203") throw std::invalid_argument("Invalid expected AGET version");
                    versions[int(index)]=QString::fromStdString(version).mid(2).toUShort(nullptr,16);
                }
                Inputs(reader,instance,s);
                if (!AgetCompiler::CompileConfigure(document,id,versions,&ignored,&failure,&c.configure) ||
                    !ClockCompiler::Compile(document,id,s.foundation.hardwareVersion,&ignored,&failure,&s.clock) ||
                    !MonitoringCompiler::Compile(document,id,mapping=="declared" ? CoBoMonitoringSettings::DeclaredParameters : CoBoMonitoringSettings::LegacyCallArguments,&ignored,&failure,&s.monitoring) ||
                    !CalibrationCompiler::Compile(document,id,versions,delay=="global" ? CoBoCalibrationSettings::GlobalDelayField : CoBoCalibrationSettings::PerAsAdDelayFields,&ignored,&failure,&s.calibration,Bool(p.At("calibration_aget_register"))))
                    throw std::runtime_error(failure.toStdString());
                c.configure.readout.bufferLayout=s.foundation.bufferLayout; s.clock.verifyHardwareVersion=s.foundation.verifyHardwareVersion;
                s.clock.allowNominalDifference=Bool(p.At("clock_allow_nominal_difference")); s.clock.allowUnverifiedUpstream=Bool(p.At("clock_allow_unverified_upstream"));
                s.monitoring.ledPolicy=led=="required" ? CoBoMonitoringSettings::RequiredLedManager : CoBoMonitoringSettings::NoLedManager;
                s.monitoring.ledHardwareVersion=s.foundation.hardwareVersion; s.adcDoubleDataRate=adc=="DDR";
                a.settings.cobos.push_back(std::move(c));
            }
        }
        if (!cobos.empty() || !mutants.empty()) throw std::invalid_argument("System profile contains undeclared instances");
        for (const auto &source:reader.Sources()) {
            const auto object=source.toObject(); ExperimentConfigurationSource s;
            s.path=object["path"].toString().toStdString(); s.sha256=object["sha256"].toString().toStdString(); a.sources.push_back(std::move(s));
        }
        a.configurationSha256=SystemExperimentDigest(a.settings);
        // Producer checks its own exact publication bytes through the same strict reader used by the native backend.
        *output=DecodeSystemExperimentArtifact(EncodeSystemExperimentArtifact(a)); error->clear(); return true;
    } catch (const std::exception &e) { *error=QString::fromUtf8(e.what()); return false; }
}
bool SystemExperimentCompiler::Save(const XcfgDocument &document,const QString &profilePath,const QString &outputPath,QString *error) {
    QFile profile(profilePath);
    if (!profile.open(QIODevice::ReadOnly)) { *error="Cannot read system profile: "+profile.errorString(); return false; }
    const auto input=profile.read(1024*1024+1);
    if (profile.error()!=QFile::NoError || !profile.atEnd()) { *error="Unreadable/excessive system profile"; return false; }
    SystemExperimentArtifact a; if (!Compile(document,input,&a,error)) return false;
    const auto bytes=QByteArray::fromStdString(EncodeSystemExperimentArtifact(a)); QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly|QIODevice::NewOnly)) { *error="Cannot create new system artifact: "+output.errorString(); return false; }
    if (output.write(bytes)!=bytes.size() || !output.flush()) { *error=output.errorString(); output.close(); QFile::remove(outputPath); return false; }
    error->clear(); return true;
}
