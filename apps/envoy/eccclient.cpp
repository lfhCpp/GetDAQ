#include "eccclient.h"
#include "protocol.h"
#include <QTimer>
#include <QUrl>

EccClient::EccClient(const NodeConfig &node, const RunConfig &config, QObject *parent)
    : QObject(parent), fNode(node), fConfig(config), fHttp(config.simulation ? nullptr : new HttpClient(this)) {}

bool EccClient::Send(const QString &operation)
{
    if (fBusy) return false;
    fBusy = true;
    if (fConfig.simulation) {
        const int generation = fGeneration;
        QTimer::singleShot(40, this, [this, operation, generation]() {
            if (generation != fGeneration) return;
            QString error;
            if (operation == "Describe" && fSimState == 1) fSimState = 2;
            else if (operation == "Prepare" && fSimState == 2) fSimState = 3;
            else if (operation == "Configure" && fSimState == 3) fSimState = 4;
            else if (operation == "Start" && fSimState == 4) fSimState = 5;
            else if (operation == "Stop" && fSimState == 5) fSimState = 4;
            else if (operation == "Stop" && fSimState == 6) fSimState = 4;
            else if (operation == "Pause" && fSimState == 5) fSimState = 6;
            else if (operation == "Resume" && fSimState == 6) fSimState = 5;
            else if (operation == "Breakup" && fSimState == 4) fSimState = 3;
            else if (operation == "Undo" && (fSimState == 2 || fSimState == 3)) --fSimState;
            else if (operation != "GetState") error = "Invalid simulated transition.";
            fBusy = false;
            emit ReplySignal(fNode.id, operation, fSimState, 0, error);
        });
    } else {
        QUrl url;
        url.setScheme("http"); url.setHost(fNode.host); url.setPort(fNode.eccPort); url.setPath("/");
        // SOAP 1.1 over HTTP requires SOAPAction. The archived gSOAP interface
        // declares no per-operation action, so the interoperable value is "".
        const QMap<QByteArray, QByteArray> headers = {{"SOAPAction", "\"\""}};
        // Hardware transitions can take much longer than a read-only status poll.
        // Use the configured transition deadline without retrying timed-out writes.
        const int timeout = operation == "GetState" ? fConfig.timeoutMs : fConfig.stepTimeoutMs;
        fHttp->Request(url, Protocol::EccRequest(fNode, fConfig.experiment, operation), "text/xml; charset=utf-8", timeout,
            [this, operation](const QByteArray &body, const QString &failure) {
                QString error = failure;
                int state = 0, transition = 0;
                if (error.isEmpty()) Protocol::ParseEcc(body, operation == "GetState", &state, &transition, &error);
                fBusy = false;
                emit ReplySignal(fNode.id, operation, state, transition, error);
            }, headers);
    }
    return true;
}

void EccClient::Cancel() { ++fGeneration; if (fHttp) fHttp->Cancel(); fBusy = false; }
