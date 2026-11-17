#include "eccclient.h"
#include "protocol.h"
#include <QUrl>

EccClient::EccClient(const NodeConfig &node, const RunConfig &config, QObject *parent)
    : QObject(parent), fNode(node), fConfig(config), fHttp(new HttpClient(this)) {}

bool EccClient::Send(const QString &operation)
{
    // Reject obsolete simulation configurations even when this client is used directly.
    if (fBusy || fConfig.simulation) return false;
    fBusy = true;
    QUrl url;
    url.setScheme("http"); url.setHost(fNode.host); url.setPort(fNode.eccPort); url.setPath("/");
    const QMap<QByteArray, QByteArray> headers = {{"SOAPAction", "\"\""}};
    const int timeout = operation == "GetState" ? fConfig.timeoutMs : fConfig.stepTimeoutMs;
    fHttp->Request(url, Protocol::EccRequest(fNode, fConfig.experiment, operation), "text/xml; charset=utf-8", timeout,
        [this, operation](const QByteArray &body, const QString &failure) {
            QString error = failure;
            int state = 0, transition = 0;
            if (error.isEmpty()) Protocol::ParseEcc(body, operation == "GetState", &state, &transition, &error);
            fBusy = false;
            emit ReplySignal(fNode.id, operation, state, transition, error);
        }, headers);
    return true;
}

void EccClient::Cancel() { fHttp->Cancel(); fBusy = false; }
