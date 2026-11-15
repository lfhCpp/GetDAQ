#include "mcpstdio.h"

#include <QJsonDocument>

#include <iostream>

namespace GetDAQControlMcpStdio {
namespace {
const size_t MaximumMessageBytes = 1024 * 1024;
}

QJsonObject RpcError(const QJsonValue &id, int code, const QString &message)
{
    QJsonObject error; error.insert("code", code); error.insert("message", message);
    QJsonObject response; response.insert("jsonrpc", "2.0"); response.insert("id", id); response.insert("error", error);
    return response;
}

void WriteMessage(const QJsonObject &message)
{
    const QByteArray bytes = QJsonDocument(message).toJson(QJsonDocument::Compact);
    std::cout.write(bytes.constData(), bytes.size()); std::cout.put('\n'); std::cout.flush();
}

bool ReadLine(std::string *line, bool *tooLarge)
{
    line->clear(); *tooLarge = false;
    bool received = false;
    char value = 0;
    while (std::cin.get(value)) {
        received = true;
        if (value == '\n') return true;
        if (line->size() < MaximumMessageBytes) line->push_back(value);
        else *tooLarge = true;
    }
    return received;
}

}
