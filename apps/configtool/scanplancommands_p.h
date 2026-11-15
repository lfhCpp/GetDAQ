#ifndef GETDAQ_CONFIGTOOL_SCANPLANCOMMANDS_P_H
#define GETDAQ_CONFIGTOOL_SCANPLANCOMMANDS_P_H

#include "systemscanplan.h"
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <functional>
#include <string>

namespace ConfigToolScanPlanCommandsInternal {
bool SavePlanFile(const QString &path, const QByteArray &bytes, QString *error);
bool ParseDecimal(const QString &text, unsigned int maximum, unsigned int *value);
std::string ReadScanPlanFile(const QString &path, qint64 maximum);
bool SaveNewMetadataFile(const QString &path, const QByteArray &bytes, QString *error,
                         const QString &description);
int HandlePeakingScanPlanCommand(const QStringList &args);
int HandlePulserScanPlanCommand(const QStringList &args);
int HandleThresholdScanPlanCommand(const QStringList &args);
int HandleAutoGainScanPlanCommand(const QStringList &args);
void ValidateGeneratedScanPlanTarget(const QString &targetPath);
void PublishGeneratedScanPlan(const QString &targetPath, const QString &stagingName,
                              const std::function<QJsonObject(const QString &)> &writePlan);
GetDAQ::SystemScanPlanSummary ValidateScanPlanDirectory(const QString &directory,
                                                         std::string *manifest);
}

namespace ConfigToolScanPlanCommands {
int HandleMetadataCommands(const QStringList &args);
int HandleGeneratedScanPlanCommands(const QStringList &args);
}

#endif
