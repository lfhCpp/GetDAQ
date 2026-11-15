#ifndef GETDAQ_EXISTING_ECC_CONNECTION_H
#define GETDAQ_EXISTING_ECC_CONNECTION_H
#include "runconfig.h"
#include <QStringList>
struct ExistingEccResult {RunConfig configuration;QString protocol,status,detail;bool portChanged=false;};
// Read-only discovery: passive Ice greeting, HTTP identity/status and SOAP queries.
ExistingEccResult CheckExistingEcc(const QString &name,const QString &address,int port,const RunConfig &base,const QStringList &profiles={},const QString &explicitProfile=QString());
QStringList KnownEccConnectionProfiles();
void RememberEccConnection(const QString &path);
#endif
