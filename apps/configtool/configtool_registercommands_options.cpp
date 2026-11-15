#include "configtool_registercommands_options_p.h"
#include "../../common/experiment/coboreadoutconfiguration.h"
#include <cstdint>
#include <exception>

namespace ConfigToolRegisterCommands {
bool Options::isRegisterCommand() const
{
    return hardware || aget || cobo || readout || configure || clock || monitoring || calibration || mutant;
}

bool ParseOptions(const QStringList &args, Options *options, QString *error)
{
    if (!options || !error) return false;
    *options = Options{};
    *error = QString();
    Options &o = *options;
    o.hardware = args.size() == 4 && args[1] == "compile-hardware";
    o.aget = args.size() == 8 && args[1] == "compile-aget";
    o.cobo = args.size() == 6 && args[1] == "compile-cobo-aget";
    o.readout = args.size() == 6 && args[1] == "compile-cobo-readout";
    o.configure = args.size() == 6 && args[1] == "compile-cobo-configure";
    o.clock = args.size() == 6 && args[1] == "compile-cobo-clock";
    o.monitoring = args.size() == 6 && args[1] == "compile-cobo-monitoring";
    o.calibration = args.size() == 7 && args[1] == "compile-cobo-calibration";
    o.mutant = args.size() == 10 && args[1] == "compile-mutant";
    if (!o.isRegisterCommand()) return false;

    if (o.mutant) {
        bool ok = false;
        o.mutantOptions.moduleCount = args[4].toUInt(&ok, 10);
        if (!ok || !o.mutantOptions.moduleCount || o.mutantOptions.moduleCount > 4096 || QString::number(o.mutantOptions.moduleCount) != args[4]) {
            *error = "Invalid MuTaNT module count; use canonical decimal 1..4096"; return false;
        }
        try {
            o.mutantOptions.gmcShelfMask = GetDAQ::ParseCoBoUnsigned(args[5].toStdString(), "GMC_MASK", 4095);
            o.mutantOptions.wscaShelfMask = GetDAQ::ParseCoBoUnsigned(args[6].toStdString(), "WSCA_MASK", 4095);
        } catch (const std::exception &e) { *error = QString::fromLocal8Bit(e.what()); return false; }
        if (args[7] == "legacy-names") o.mutantOptions.layout = GetDAQ::MutantSettings::LegacyNamesV15;
        else if (args[7] != "modern") { *error = "Invalid MuTaNT layout; use modern or legacy-names"; return false; }
        if (args[8] == "legacy-vhdci") o.mutantOptions.clockPolicy = GetDAQ::MutantSettings::LegacyVhdciFallback;
        else if (args[8] != "strict") { *error = "Invalid MuTaNT clock policy; use strict or legacy-vhdci"; return false; }
    }
    if (o.calibration) {
        if (args[5] == "per-asad") o.delayLayout = GetDAQ::CoBoCalibrationSettings::PerAsAdDelayFields;
        else if (args[5] != "global") { *error = "Invalid delay layout; use global or per-asad"; return false; }
    }
    if (o.monitoring) {
        if (args[4] == "legacy") o.mapping = GetDAQ::CoBoMonitoringSettings::LegacyCallArguments;
        else if (args[4] != "declared") { *error = "Invalid monitoring mode mapping; use declared or legacy"; return false; }
    }
    if (o.clock) {
        bool ok = false; o.hardwareVersion = args[4].toUInt(&ok, 10);
        if (!ok || o.hardwareVersion == 0 || o.hardwareVersion == UINT32_MAX || QString::number(o.hardwareVersion) != args[4]) {
            *error = "Invalid hardware version; use a canonical positive decimal integer"; return false;
        }
    }
    if (o.cobo || o.readout || o.configure || o.calibration) {
        const auto entries = args[4].split(','); bool valid = !entries.isEmpty() && entries.size() <= 16;
        for (const auto &entry : entries) {
            const auto pair = entry.split('=');
            if (pair.size() != 2) { valid = false; break; }
            bool ok = false; const int id = pair[0].toInt(&ok, 10);
            if (!ok || id < 0 || id > 15 || QString::number(id) != pair[0] || o.versions.contains(id)
                || (pair[1] != "0x201" && pair[1] != "0x202" && pair[1] != "0x203")) { valid = false; break; }
            o.versions[id] = pair[1].mid(2).toUShort(nullptr, 16);
        }
        if (!valid) { *error = "Invalid/duplicate CoBo version map; use 0..15=0x201|0x202|0x203 entries"; return false; }
    }
    if (o.aget) {
        const auto id = [](const QString &text, unsigned int *value) {
            if (text.size() != 1 || text[0] < '0' || text[0] > '3') return false;
            *value = text[0].unicode() - '0'; return true;
        };
        if (!id(args[4], &o.asadId) || !id(args[5], &o.agetId)
            || (args[6] != "0x201" && args[6] != "0x202" && args[6] != "0x203")) {
            *error = "AsAd/AGET IDs must be 0..3; chip version must be 0x201, 0x202 or 0x203"; return false;
        }
        o.chipVersion = args[6].mid(2).toUShort(nullptr, 16);
    }
    return true;
}
}
