#ifndef GETDAQ_EXPORTER_SOURCES_H
#define GETDAQ_EXPORTER_SOURCES_H
#include "runconfig.h"
#include <QJsonArray>
// Preview endpoints are separate from the legacy secondary board data target.
QJsonArray ExporterSources(const RunConfig &configuration,const QVector<NodeStatus> &observations={});
#endif
