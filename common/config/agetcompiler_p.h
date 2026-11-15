#ifndef GETDAQ_AGETCOMPILER_P_H
#define GETDAQ_AGETCOMPILER_P_H

#include "experimentreader.h"
#include "../experiment/agetconfiguration.h"

#include <QJsonObject>

namespace AgetCompilerInternal {

class Compiler : public ExperimentReader {
public:
    GetDAQ::AgetSettings lastSettings;

    explicit Compiler(const XcfgDocument &document);
    QJsonObject Run(const QString &coboId, unsigned int asadId,
                    unsigned int agetId, quint16 chipVersion);
};

bool SaveJson(const QJsonObject &compiled, const QString &path, QString *error);

}

#endif
