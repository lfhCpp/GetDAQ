#include "scanplancommands.h"
#include "scanplancommands_p.h"

int ConfigToolScanPlanCommands::Handle(const QStringList &args)
{
    const int metadata = HandleMetadataCommands(args);
    if (metadata != -1) return metadata;
    return HandleGeneratedScanPlanCommands(args);
}
