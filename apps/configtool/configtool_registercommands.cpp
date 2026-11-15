#include "configtool_registercommands_p.h"
#include "configtool_registercommands_options_p.h"
#include "hardwarecompiler.h"
#include "agetcompiler.h"
#include "clockcompiler.h"
#include "monitoringcompiler.h"
#include "calibrationcompiler.h"
#include "mutantcompiler.h"
#include <QTextStream>

namespace ConfigToolRegisterCommands {
int Handle(const QStringList &args)
{
    Options options; QString parseError;
    if (!ParseOptions(args, &options, &parseError)) {
        if (!parseError.isEmpty()) { QTextStream(stderr) << parseError << '\n'; return 2; }
        QTextStream(stderr) << "Usage: getdaq-config compile-hardware INPUT.xcfg OUTPUT.json\n"
                            << "       getdaq-config create-scan-policy PLAN_DIR event-count EVENTS_PER_POINT MAX_DURATION_MS NEW_POLICY.json\n"
                            << "       getdaq-config create-scan-snapshot PLAN_DIR POLICY.json EXECUTION_ID NEW_SNAPSHOT.json\n"
                              "       getdaq-config import-binary INPUT.bcfg NEW_OUTPUT.xcfg\n"
                              "       getdaq-config export-binary INPUT.xcfg OBJECT_PATH declarations|effective NEW_OUTPUT.bcfg\n"
                              "       getdaq-config snapshot INPUT.xcfg OUTPUT.json\n"
                              "       getdaq-config export-xml INPUT.xcfg OBJECT_PATH declarations|effective NEW_OUTPUT.xcfg\n"
                              "       getdaq-config preview-object-override INPUT.xcfg INHERITED_OBJECT_PATH\n"
                              "       getdaq-config override-object INPUT.xcfg INHERITED_OBJECT_PATH\n"
                              "       getdaq-config preview-workspace-copy INPUT.xcfg shared|isolated NEW_DIRECTORY\n"
                              "       getdaq-config copy-workspace INPUT.xcfg shared|isolated NEW_DIRECTORY\n"
                              "       getdaq-config publish-system INPUT.xcfg PROFILE.json NEW_DIRECTORY\n"
                              "       getdaq-config diff-snapshot BEFORE.json AFTER.json REVIEW.json\n"
                              "       getdaq-config compile-system INPUT.xcfg PROFILE.json OUTPUT.json\n"
                              "       getdaq-config plan-peaking-scan SYSTEM.json COBO_ID NEW_OUTPUT_DIRECTORY\n"
                              "       getdaq-config plan-pulser-scan SYSTEM.json COBO_ID START_MV STOP_MV STEP_MV NEW_OUTPUT_DIRECTORY\n"
                              "       getdaq-config plan-threshold-scan SYSTEM.json COBO_ID ASAD_ID AGET_ID START_CODE STOP_CODE STEP_CODE NEW_OUTPUT_DIRECTORY\n"
                              "       getdaq-config plan-autogain-scan SYSTEM.json COBO_ID ASAD_ID AGET_ID FIRST_CHANNEL LAST_CHANNEL START_MV STOP_MV STEP_MV NEW_OUTPUT_DIRECTORY\n"
                              "       getdaq-config compile-aget INPUT.xcfg COBO_INSTANCE ASAD_ID AGET_ID CHIP_VERSION OUTPUT.json\n"
                              "       getdaq-config compile-cobo-aget INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json\n"
                              "       getdaq-config compile-cobo-readout INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json\n"
                              "       getdaq-config compile-cobo-configure INPUT.xcfg COBO_INSTANCE VERSIONS OUTPUT.json\n"
                              "       getdaq-config compile-cobo-clock INPUT.xcfg COBO_INSTANCE HARDWARE_VERSION OUTPUT.json\n"
                              "       getdaq-config compile-cobo-monitoring INPUT.xcfg COBO_INSTANCE MODE_MAPPING OUTPUT.json\n"
                              "       getdaq-config compile-cobo-calibration INPUT.xcfg COBO_INSTANCE VERSIONS DELAY_LAYOUT OUTPUT.json\n"
                              "       getdaq-config compile-mutant INPUT.xcfg MUTANT_INSTANCE MODULE_COUNT GMC_MASK WSCA_MASK REGISTER_LAYOUT CLOCK_POLICY OUTPUT.json\n"
                              "       getdaq-config preview-path-migration OLD_SOURCE NEW_SOURCE ROOT.xcfg [OTHER_ROOT.xcfg ...]\n"
                              "       getdaq-config discover-project-referrers SCAN_DIRECTORY OLD_SOURCE\n"
                              "       getdaq-config migrate-path OLD_SOURCE NEW_SOURCE ROOT.xcfg [OTHER_ROOT.xcfg ...]\n"
                              "       getdaq-config preview-path-recovery MOVE_RECORD_DIRECTORY original|candidate\n"
                              "       getdaq-config recover-path MOVE_RECORD_DIRECTORY original|candidate\n"
                              "Path migration changes local configuration files; add all affected roots. Destination must be absent; no hardware commands.\n"
                              "REGISTER_LAYOUT: modern or legacy-names (v1.5 semantics); CLOCK_POLICY: strict or legacy-vhdci.\n"
                              "MODULE_COUNT: canonical decimal 1..4096; masks: 12-bit decimal or 0x hex, caller-specified and not alignment-verified.\n"
                              "DELAY_LAYOUT: global or per-asad; explicit matching hardware layout required.\n"
                              "MODE_MAPPING: declared (correct parameter order) or legacy (old reordered call arguments); explicit choice required.\n"
                              "HARDWARE_VERSION: positive decimal; 1 = R-CoBo, >=2 = M-CoBo; caller-specified, not verified.\n"
                              "plan-peaking-scan is offline only; COBO_ID selects exactly one enabled CoBo; creates 16 immutable AGET/ASTRE-specific system artifacts and never overwrites an existing directory.\n"
                              "plan-pulser-scan is offline only; integer mV, ascending inclusive interval, 1..256 points, stop must match a step; selected CoBo start must equal START_MV.\n"
                              "plan-threshold-scan is offline only; explicit active AGET, packed 7-bit code 0..127, ascending inclusive range, 1..128 points; START_CODE coarse bits must match the baseline global threshold.\n"
                              "plan-autogain-scan is offline only; selected active channels x inclusive integer mV range, at most 256 points; every point is a full system configuration and requires a separate Configure/Run. No gain judgment is made.\n"
                              "VERSIONS: global-chip-id=version,... (e.g. 0=0x202,1=0x202); cover exactly all active chips.\n"
                              "CHIP_VERSION: 0x201, 0x202 or 0x203; output must be new. Offline only; no hardware commands.\n";
        return args.size() == 2 && args[1] == "--help" ? 0 : 2;
    }
    const bool hardware = options.hardware, cobo = options.cobo;
    const bool readout = options.readout, configure = options.configure, clock = options.clock;
    const bool monitoring = options.monitoring, calibration = options.calibration, mutant = options.mutant;
    XcfgDocument document; QString error;
    if (!document.Load(args[2], &error)
        || (hardware ? !HardwareCompiler::Save(document, args[3], &error)
                     : mutant ? !MutantCompiler::Save(document,args[3],options.mutantOptions,args[9],&error)
                     : calibration ? !CalibrationCompiler::Save(document,args[3],options.versions,options.delayLayout,args[6],&error)
                     : monitoring ? !MonitoringCompiler::Save(document,args[3],options.mapping,args[5],&error)
                     : clock ? !ClockCompiler::Save(document,args[3],options.hardwareVersion,args[5],&error)
                     : configure ? !AgetCompiler::SaveConfigure(document, args[3], options.versions, args[5], &error)
                     : readout ? !AgetCompiler::SaveReadout(document, args[3], options.versions, args[5], &error)
                     : cobo ? !AgetCompiler::SaveCoBo(document, args[3], options.versions, args[5], &error)
                            : !AgetCompiler::Save(document, args[3], options.asadId, options.agetId, options.chipVersion, args[7], &error))) {
        QTextStream(stderr) << error << '\n'; return 1;
    }
    QTextStream(stdout) << (hardware ? "Compiled hardware description: " : "Compiled offline register image: ") << (hardware ? args[3] : mutant ? args[9] : calibration ? args[6] : (cobo || readout || configure || clock || monitoring) ? args[5] : args[7]) << '\n';
    return 0;
}
}
