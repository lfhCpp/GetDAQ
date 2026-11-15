#ifndef GETDAQ_CONFIGTOOL_REGISTERCOMMANDS_OPTIONS_P_H
#define GETDAQ_CONFIGTOOL_REGISTERCOMMANDS_OPTIONS_P_H

#include "../../common/experiment/cobocalibrationconfiguration.h"
#include "../../common/experiment/cobomonitoringconfiguration.h"
#include "../../common/experiment/mutantconfiguration.h"
#include <QMap>
#include <QString>
#include <QStringList>

namespace ConfigToolRegisterCommands {
struct Options {
    bool hardware = false, aget = false, cobo = false, readout = false;
    bool configure = false, clock = false, monitoring = false;
    bool calibration = false, mutant = false;
    unsigned int asadId = 0, agetId = 0;
    quint16 chipVersion = 0;
    quint32 hardwareVersion = 0;
    GetDAQ::CoBoMonitoringSettings::ModeMapping mapping = GetDAQ::CoBoMonitoringSettings::DeclaredParameters;
    GetDAQ::CoBoCalibrationSettings::DelayLayout delayLayout = GetDAQ::CoBoCalibrationSettings::GlobalDelayField;
    GetDAQ::MutantSettings mutantOptions;
    QMap<int, quint16> versions;

    bool isRegisterCommand() const;
};

// Parse only compiler subcommands. Returns false and supplies the existing CLI
// diagnostic when a recognized command has invalid arguments.
bool ParseOptions(const QStringList &args, Options *options, QString *error);
}

#endif
