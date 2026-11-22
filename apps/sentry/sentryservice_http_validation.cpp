#include "sentryservice_http_p.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace SentryHttpInternal {
namespace {
bool Reject(int code, const QString &message, int *errorCode, QString *error)
{
    *errorCode = code;
    *error = message;
    return false;
}

bool IsOperationId(const QString &value)
{
    return QRegularExpression("^[A-Za-z0-9_-]{1,80}$").match(value).hasMatch();
}
}

bool ResolveRoute(const QString &method, const QString &path, Route *route,
                  int *errorCode, QString *error)
{
    if (path == "/status") *route = Route::Status;
    else if (path == "/catalog") *route = Route::Catalog;
    else if (path == "/catalog/finalize") *route = Route::FinalizeReplacement;
    else return Reject(404, "Unknown route.", errorCode, error);

    const bool mutation = *route != Route::Status;
    if (method != (mutation ? "POST" : "GET"))
        return Reject(405, "Method not allowed.", errorCode, error);
    return true;
}

bool ParseMutation(Route route, const QByteArray &body, MutationRequest *request,
                   int *errorCode, QString *error)
{
    if (route == Route::Status) return true;

    QJsonParseError parse;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parse);
    const QJsonObject object = document.object();
    const QJsonValue number = object.value("run_number");
    if (parse.error != QJsonParseError::NoError || !document.isObject() ||
        !object.value("experiment").isString() || !number.isDouble() ||
        number.toDouble() < 0 || number.toDouble() >= std::numeric_limits<int>::max() ||
        number.toDouble() != std::floor(number.toDouble())) {
        return Reject(400, "Expected experiment string and nonnegative integer run_number.", errorCode, error);
    }

    request->experiment = object.value("experiment").toString();
    request->run = number.toInt();
    if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$")
             .match(request->experiment).hasMatch()) {
        return Reject(400, "Invalid experiment.", errorCode, error);
    }

    const bool catalog = route == Route::Catalog;
    if (catalog && object.contains("replace_completed_run")) {
        if (!object.value("replace_completed_run").isBool())
            return Reject(400, "replace_completed_run must be boolean.", errorCode, error);
        request->replaceCompletedRun = object.value("replace_completed_run").toBool();
    }
    if(catalog && object.contains("overwrite_existing_run")){
        if(!object["overwrite_existing_run"].isBool())return Reject(400,"overwrite_existing_run must be boolean.",errorCode,error);
        request->overwriteExisting=object["overwrite_existing_run"].toBool();
        if(request->overwriteExisting && !request->replaceCompletedRun)return Reject(400,"Overwrite requires explicit replacement confirmation.",errorCode,error);
    }
    if(catalog && object.contains("append_existing_run")){
        if(!object["append_existing_run"].isBool())return Reject(400,"append_existing_run must be boolean.",errorCode,error);
        request->appendExisting=object["append_existing_run"].toBool();
        if(request->appendExisting && (!request->replaceCompletedRun || request->overwriteExisting))return Reject(400,"Append requires explicit confirmation and cannot also overwrite.",errorCode,error);
    }
    if (object.contains("operation_id")) {
        if (!object.value("operation_id").isString())
            return Reject(400, "operation_id must be a string.", errorCode, error);
        request->operationId = object.value("operation_id").toString();
    }
    if (catalog && request->replaceCompletedRun && !IsOperationId(request->operationId))
        return Reject(400, "Explicit replacement requires a bounded operation_id.", errorCode, error);
    if (catalog && !request->replaceCompletedRun && !request->operationId.isEmpty())
        return Reject(400, "operation_id requires explicit replacement.", errorCode, error);
    if (route == Route::FinalizeReplacement && !IsOperationId(request->operationId))
        return Reject(400, "Replacement finalization requires a bounded operation_id.", errorCode, error);
    return true;
}
}
