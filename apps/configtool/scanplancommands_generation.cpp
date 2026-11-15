#include "scanplancommands_p.h"

int ConfigToolScanPlanCommands::HandleGeneratedScanPlanCommands(const QStringList &args)
{
    if (args.size() < 2) return -1;
    if (args[1] == "plan-peaking-scan") return ConfigToolScanPlanCommandsInternal::HandlePeakingScanPlanCommand(args);
    if (args[1] == "plan-pulser-scan") return ConfigToolScanPlanCommandsInternal::HandlePulserScanPlanCommand(args);
    if (args[1] == "plan-threshold-scan") return ConfigToolScanPlanCommandsInternal::HandleThresholdScanPlanCommand(args);
    if (args[1] == "plan-autogain-scan") return ConfigToolScanPlanCommandsInternal::HandleAutoGainScanPlanCommand(args);
    return -1;
}
