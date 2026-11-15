#ifndef GETDAQ_CALIBRATIONCOMPILER_H
#define GETDAQ_CALIBRATIONCOMPILER_H
#include "xcfgdocument.h"
#include "../experiment/cobocalibrationconfiguration.h"
#include <QJsonObject>
#include <QMap>
class CalibrationCompiler {
public:
    static bool Compile(const XcfgDocument &document, const QString &coboId, const QMap<int,quint16> &versions,
                        GetDAQ::CoBoCalibrationSettings::DelayLayout layout, QJsonObject *output, QString *error,
                        GetDAQ::CoBoCalibrationSettings *settings = nullptr, bool calibrationAgetRegister = true);
    static bool Save(const XcfgDocument &document, const QString &coboId, const QMap<int,quint16> &versions,
                     GetDAQ::CoBoCalibrationSettings::DelayLayout layout, const QString &path, QString *error);
};
#endif
