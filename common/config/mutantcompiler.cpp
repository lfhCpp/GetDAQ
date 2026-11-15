// Adapted from CEA GET MutantConfigParser (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "mutantcompiler.h"
#include "experimentreader.h"
#include "../experiment/coboreadoutconfiguration.h"
#include "../experiment/cobomonitoringconfiguration.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>
namespace {
bool Exists(const XcfgDocument &document, const QString &path)
{
    QString error; if (document.Lookup(path,&error) >= 0) return true;
    if (!error.startsWith("No declared instance/value for ")) throw std::runtime_error(error.toStdString());
    return false;
}
class Reader : public ExperimentReader {
public:
    explicit Reader(const XcfgDocument &d) : ExperimentReader(d) {}
    unsigned int UInt(const QString &path, unsigned int maximum, const QString &fallback = QString(),
                      const QString &unit = QString(), bool requireHexType = false)
    {
        const QString text = Scalar(path,fallback,unit,true);
        if (requireHexType) {
            QString error; const int index = document.Lookup(path,&error);
            if (index >= 0) {
                const auto &node = document.Nodes().at(index); const auto type = node.attributes.value("type");
                // CompoundConfig Scalar::getHexValue是严格类型访问，不是十进制转换。
                if (type != "hex" && !(type.isEmpty() && node.value.startsWith("0x",Qt::CaseInsensitive)))
                    throw std::invalid_argument((path+": expected type=hex or an untyped 0x-prefixed hexadecimal value").toStdString());
            }
        }
        return GetDAQ::ParseCoBoUnsigned(text.toStdString(),path.toStdString(),maximum);
    }
    double Real(const QString &path)
    { return GetDAQ::ParseMonitoringReal(Scalar(path,QString(),"us",false,true).toStdString(),path.toStdString()); }
    void Ids(const QString &path, const QString &name, unsigned int first, unsigned int last, bool optional = false)
    {
        if (optional && !Exists(document,path)) return;
        const auto children = Children(path,name);
        if (!optional && children.size() != int(last-first+1)) throw std::invalid_argument((path+": missing concrete "+name+" IDs").toStdString());
        for (const auto &child : children) {
            const auto text = child.mid(child.lastIndexOf('[')+1).chopped(1);
            bool ok = false; const auto id = text.toUInt(&ok,10);
            if (!ok || id < first || id > last || QString::number(id) != text)
                throw std::invalid_argument(("Invalid "+name+" ID at "+child).toStdString());
        }
    }
};
void Register(QJsonArray &registers, const char *device, const QString &name, unsigned int value, const QString &field = QString())
{
    QJsonObject r; r["device"] = device; r["register"] = name;
    if (!field.isEmpty()) r["field"] = field;
    r["value"] = QString::number(value); r["hex"] = "0x" + QString::number(value,16); registers.append(r);
}
QJsonObject Timing(double requested, unsigned int code, double unit)
{
    QJsonObject t; t["requested_us"] = requested; t["code"] = int(code); t["nominal_us"] = code*unit;
    t["physical_verified"] = false; return t;
}
}
bool MutantCompiler::Compile(const XcfgDocument &document, const QString &instance, const GetDAQ::MutantSettings &options,
                             QJsonObject *output, QString *error, GetDAQ::MutantSettings *settingsOut)
{
    try {
        Reader reader(document); const auto path = reader.MutantPath(instance);
        GetDAQ::MutantSettings s;
        s.layout = options.layout; s.clockPolicy = options.clockPolicy; s.moduleCount = options.moduleCount;
        s.gmcShelfMask = options.gmcShelfMask; s.wscaShelfMask = options.wscaShelfMask;
        s.twoProtonCounterCommand = options.twoProtonCounterCommand; s.allowPrototypePowerDf = options.allowPrototypePowerDf;
        s.verifyBoardId = options.verifyBoardId; s.expectedBoardId = options.expectedBoardId;
        s.isSlave = reader.Boolean(path+"/isSlave");
        if (s.isSlave) {
            s.slaveId = reader.UInt(path+"/Slave/slaveId",1);
            const auto port = reader.Scalar(path+"/Slave/slavePort");
            if (port != "A" && port != "B") throw std::invalid_argument("MuTaNT slavePort must be A or B");
            s.slavePortA = port == "A";
        } else {
            s.masterPortAEnabled = reader.Boolean(path+"/Master/enablePortA");
            s.masterPortBEnabled = reader.Boolean(path+"/Master/enablePortB");
        }
        const char *words[] = {"low","mid","high"};
        for (unsigned int i = 0; i < 3; ++i)
            s.timestamp10ns |= std::uint64_t(reader.UInt(path+"/preLoadedTimestamp/"+words[i],65535,QString(),QString(),true)) << (16*i);
        s.eventNumber = reader.UInt(path+"/preLoadedEventNumber",UINT32_MAX,QString(),QString(),true);
        s.preloadTimestamp = reader.Boolean(path+"/preLoadTimestamp","true");
        s.preloadEventNumber = reader.Boolean(path+"/preLoadEventNumber","true");
        s.twoProtonMode = reader.Boolean(path+"/TwoProtonDecayMode/isEnabled");
        if (s.twoProtonMode) s.halfEventReadout = reader.Boolean(path+"/TwoProtonDecayMode/halfEventReadout");
        const auto priority = reader.Scalar(path+"/levelPriority");
        if (priority != "L0L1" && priority != "L1L0") throw std::invalid_argument("MuTaNT levelPriority must be L0L1 or L1L0");
        s.level1BeforeLevel0 = priority == "L1L0";
        reader.Ids(path,"Level",0,2);
        const auto level0 = path+"/Level[0]", level1 = path+"/Level[1]", level2 = path+"/Level[2]";
        s.level0Enabled = reader.Boolean(level0+"/isEnabled");
        s.level0GateDelayUs = reader.Real(level0+"/gateDelay"); s.level0GateDurationUs = reader.Real(level0+"/gateDuration");
        s.level1Enabled = reader.Boolean(level1+"/isEnabled"); s.level1Selection = reader.Scalar(level1+"/selection").toStdString();
        for (unsigned int i = 0; i < (s.twoProtonMode ? 2U : 1U); ++i) {
            const auto trigger = level1 + (i ? "/L1B" : "/L1A");
            s.multiplicityThresholds[2*i] = reader.UInt(trigger+"/lowMultThreshold",65535);
            s.multiplicityThresholds[2*i+1] = reader.UInt(trigger+"/highMultThreshold",65535);
            s.bucketCounts[2*i] = reader.UInt(trigger+"/lowNumBuckets",511,QString(),"buckets");
            s.bucketCounts[2*i+1] = reader.UInt(trigger+"/highNumBuckets",511,QString(),"buckets");
        }
        if (s.twoProtonMode) s.level1bTimeoutUs = reader.Real(level1+"/L1B/timeout");
        s.level1PostTriggerDelayUs = reader.Real(level1+"/postTriggerDelay");
        s.level2Enabled = reader.Boolean(level2+"/isEnabled"); s.hitPatternModified = reader.Boolean(level2+"/hitPatternModified");
        const auto cs = reader.Scalar(path+"/centrumScalerInterface");
        if (cs == "CENTRUM") s.centrumScalerMode = GetDAQ::MutantSettings::Centrum;
        else if (cs == "Scalers") s.centrumScalerMode = GetDAQ::MutantSettings::Scalers;
        else if (cs != "Disabled") throw std::invalid_argument("MuTaNT centrumScalerInterface must be Disabled, CENTRUM or Scalers");
        s.testMode = reader.Boolean(path+"/testModeEnabled");
        const auto scalers = path+"/Scalers";
        s.clearScalers = reader.Boolean(scalers+(Exists(document,scalers+"/clearScalers") ? "/clearScalers" : "/clearCounters"));
        s.clearCounters = reader.Boolean(path+"/clearCounters",s.clearScalers ? "true" : "false");
        reader.Ids(scalers,"enableScaler",1,5);
        for (unsigned int i = 0; i < 5; ++i) s.scalerEnabled[i] = reader.Boolean(scalers+QString("/enableScaler[%1]").arg(i+1));
        s.vcxoControl = reader.UInt(path+"/vcxoControl",255,"0x80",QString(),true);
        s.sourceId = reader.UInt(path+"/sourceId",255); s.frameType = reader.UInt(path+"/frameType",3);
        s.externalClockEnabled = s.isSlave || reader.Boolean(path+"/GlobalMasterClock/externalClockEnabled");
        s.externalClockSelection = reader.Scalar(path+"/GlobalMasterClock/externalClockSelection").toStdString();
        reader.Ids(path+"/AmcTagOnly","ATO",1,12,true);
        for (unsigned int i = 0; i < 12; ++i) if (reader.Boolean(path+QString("/AmcTagOnly/ATO[%1]").arg(i+1),"false")) s.amcTagOnly |= 1U<<i;
        s.userRegister = reader.UInt(path+"/UserRegister",65535,"0");
        const char *inspection[] = {"frontPanelInsp1","frontPanelInsp2","rearPanelInsp1","rearPanelInsp2"};
        for (unsigned int i = 0; i < 4; ++i) s.inspection[i] = reader.UInt(path+"/Inspection/"+inspection[i],255,"0");
        s.readoutEnabled = reader.Boolean(path+"/readoutModeEnabled","true");
        const auto image = GetDAQ::CompileMutant(s);
        QJsonObject result; result["schema_version"] = 1; result["kind"] = "getdaq.mutant-image"; result["mutant_instance"] = instance;
        result["register_mapping"] = "v1.5 semantics"; result["register_layout"] = s.layout == GetDAQ::MutantSettings::ModernNamesV15 ? "modern" : "legacy-names";
        result["clock_policy"] = s.clockPolicy == GetDAQ::MutantSettings::StrictClockNames ? "strict" : "legacy-vhdci";
        result["clock_source_fallback"] = image.externalClockSourceFallback; result["clock_source_code"] = int(image.externalClockSource);
        result["external_clock_enabled"] = s.externalClockEnabled;
        QJsonObject clockUpdate; clockUpdate["device"] = "GPIO_CLK_TREE"; clockUpdate["register"] = "CLOCK_TREE_CONFIG2";
        clockUpdate["field"] = "clk_path_sel"; clockUpdate["mask"] = 5; clockUpdate["value"] = int(image.clockPathValue);
        clockUpdate["requires_read_modify_write"] = true; result["clock_path_update"] = clockUpdate;
        result["module_count"] = int(s.moduleCount); result["is_slave"] = s.isSlave; result["two_proton_mode"] = s.twoProtonMode;
        result["readout_enabled"] = s.readoutEnabled; result["timestamp_10ns"] = QString::number(qulonglong(s.timestamp10ns));
        result["event_number"] = QString::number(qulonglong(s.eventNumber));
        result["preload_timestamp"] = s.preloadTimestamp; result["preload_event_number"] = s.preloadEventNumber;
        result["clear_scalers"] = s.clearScalers; result["clear_counters"] = s.clearCounters;
        result["scaler_mask"] = int(image.scalerMask); result["gmc_shelf_mask"] = int(s.gmcShelfMask); result["wsca_shelf_mask"] = int(s.wscaShelfMask);
        result["shelf_mask"] = int(image.shelfMask); result["amc_tag_only_mask"] = int(s.amcTagOnly);
        result["final_gmc_output_mask"] = int(s.gmcShelfMask | s.amcTagOnly); result["final_wsca_output_mask"] = int(s.wscaShelfMask | s.amcTagOnly);
        result["layout_topology_source"] = "caller-specified; not hardware or physical alignment verified";
        result["two_proton_counter_command"] = s.twoProtonCounterCommand; result["allow_prototype_power_df"] = s.allowPrototypePowerDf;
        result["verify_board_id"] = s.verifyBoardId; result["expected_board_id"] = int(s.expectedBoardId);
        QJsonObject timings; timings["level0_delay"] = Timing(s.level0GateDelayUs,image.level0GateDelay,.01);
        timings["level0_duration"] = Timing(s.level0GateDurationUs,image.level0GateDuration,.01);
        timings["level1_post_delay"] = Timing(s.level1PostTriggerDelayUs,image.level1PostTriggerDelay,.01);
        timings["level1b_timeout"] = Timing(s.level1bTimeoutUs,image.level1bTimeout,2.56); result["timings"] = timings;
        QJsonArray registers;
        Register(registers,"CLK200_SPI","VCXO_CONTROL",s.vcxoControl);
        Register(registers,"GPIO_SW_LEDS","TDC_STAT_CARD_B_FLGS",image.externalClockSource,"gmc");
        Register(registers,"CARD_B_EMC","GMC_WSCA_SEL",0,"GMC"); Register(registers,"CARD_B_EMC","GMC_WSCA_SEL",image.wscaSource,"WSCA");
        Register(registers,"CARD_B_EMC","GMC_WSCA_SEL",image.promptWsca,"PromptWSCA"); Register(registers,"CARD_B_EMC","GLOBAL_CONF",image.globalConfiguration);
        Register(registers,"CARD_B_EMC","TRIGGER_CTRL",s.testMode,"test"); Register(registers,"CARD_B_EMC","TRIGGER_CTRL",!s.readoutEnabled,"read_mode");
        const char *ts[] = {"SET_TIME_LOW","SET_TIME_MID","SET_TIME_HIGH"};
        for (unsigned int i = 0; i < 3; ++i) Register(registers,"CARD_B_EMC",ts[i],image.timestampWords[i]);
        Register(registers,"CARD_B_EMC","SET_EVT_NUM_LOW",image.eventWords[0]); Register(registers,"CARD_B_EMC","SET_EVT_NUM_HIGH",image.eventWords[1]);
        Register(registers,"CARD_B_EMC","SOURCE_SYSTEM",s.sourceId,"source"); Register(registers,"CARD_B_EMC","FIFO_FRAME_TYPE",s.frameType,"ft");
        Register(registers,"CARD_B_EMC",image.shelfRegister,image.shelfMask); Register(registers,"CARD_B_EMC",image.tagRegister,s.amcTagOnly); Register(registers,"CARD_B_EMC",image.userRegister,s.userRegister);
        Register(registers,"CARD_B_EMC","L0_GATE_DELAY",image.level0GateDelay); Register(registers,"CARD_B_EMC","L0_GATE_DURATION",image.level0GateDuration);
        const char *thresholds[] = {"L1_A_LOW_MULT_THRESHOLD","L1_A_HIGH_MULT_THRESHOLD","L1_B_LOW_MULT_THRESHOLD","L1_B_HIGH_MULT_THRESHOLD"};
        const char *buckets[] = {"L1_A_LOW_TH_N_BUCKETS","L1_A_HIGH_TH_N_BUCKETS","L1_B_LOW_TH_N_BUCKETS","L1_B_HIGH_TH_N_BUCKETS"};
        for (unsigned int i = 0; i < 4; ++i) { Register(registers,"CARD_B_EMC",thresholds[i],image.multiplicityThresholds[i]); Register(registers,"CARD_B_EMC",buckets[i],image.bucketCounts[i]); }
        Register(registers,"CARD_B_EMC","L1_B_TIMEOUT",image.level1bTimeout); Register(registers,"CARD_B_EMC","L1_POST_TRIG_DELAY",image.level1PostTriggerDelay);
        Register(registers,"CARD_B_EMC","FP_LOG_IREG",image.frontInspection); Register(registers,"CARD_B_EMC","RP_LOG_IREG",image.rearInspection);
        result["registers"] = registers; result["parameters"] = reader.parameters; result["sources"] = reader.Sources();
        result["hardware_deployment_supported"] = false; result["complete_prepare"] = false; result["alignment_verified"] = false;
        QJsonArray warnings;
        warnings.append("Offline register values are not an executable plan: preserve clock-path bits, command ordering, preloads and scaler enables in the native adapter");
        warnings.append("Layout and masks are caller assumptions; legacy names retain v1.5 algorithms, not v1.4 timing/selection support");
        warnings.append("No physical clock/TDC alignment, exclusive ownership, sender lifecycle or complete Prepared/Ready is proven");
        if (image.externalClockSourceFallback) warnings.append("Explicit legacy fallback: unrecognized clock source selects VHDCI; review actual wiring");
        if (s.frameType) warnings.append("Repository experiment declares only frame type 0; other 2-bit codes require firmware/data-format acceptance");
        result["warnings"] = warnings;
        if (settingsOut) *settingsOut = s;
        *output = result; error->clear(); return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}
bool MutantCompiler::Save(const XcfgDocument &document, const QString &instance, const GetDAQ::MutantSettings &options, const QString &path, QString *error)
{
    QJsonObject compiled; if (!Compile(document,instance,options,&compiled,error)) return false;
    const auto bytes = QJsonDocument(compiled).toJson();
    if (bytes.size() > 8*1024*1024) { *error = "MuTaNT artifact exceeds 8 MiB"; return false; }
    QFile file(path); if (!file.open(QIODevice::WriteOnly|QIODevice::NewOnly)) { *error = "Cannot create a new artifact: "+file.errorString(); return false; }
    if (file.write(bytes) != bytes.size() || !file.flush()) { *error = file.errorString(); file.close(); QFile::remove(path); return false; }
    error->clear(); return true;
}
