#ifndef GETDAQ_EXPERIMENTREADER_H
#define GETDAQ_EXPERIMENTREADER_H
#include "xcfgdocument.h"
#include <QJsonArray>
#include <QJsonObject>
// 共用实验标量检查：单位、范围、类型、来源和代码默认值。
class ExperimentReader {
public:
    explicit ExperimentReader(const XcfgDocument &document);
    QString Scalar(const QString &path, const QString &fallback = QString(), const QString &unit = QString(), bool allowHex = false, bool allowReal = false);
    bool Boolean(const QString &path, const QString &fallback = QString());
    QStringList Children(const QString &path, const QString &name);
    QString CoBoPath(const QString &id);
    QString MutantPath(const QString &id);
    QJsonArray Sources() const;
    const XcfgDocument &document;
    QJsonArray parameters;
};
#endif
