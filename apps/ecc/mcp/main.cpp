#include <QCoreApplication>
#include "mcpsettings.h"
#include "mcpserver_p.h"
#include "mcpstdio.h"
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace GetDAQControlMcpInternal;
using namespace GetDAQControlMcpStdio;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("getdaq-control-mcp"); app.setApplicationVersion("1.0.0");
    QString configPath;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "--config" && i + 1 < args.size()) configPath = args[++i];
        else if (args[i] == "--help") {
            std::cerr << "Usage: getdaq-control-mcp --config MCP_CONFIG.json\n";
            return 0;
        } else {
            std::cerr << "Unknown argument\n";
            return 2;
        }
    }
    if (configPath.isEmpty()) { std::cerr << "Supply --config MCP_CONFIG.json\n"; return 2; }
    try {
        Server server(GetDAQControlMcpInternal::LoadSettings(configPath));
        std::string line;
        bool tooLarge = false;
        while (ReadLine(&line, &tooLarge)) {
            if (tooLarge) {
                WriteMessage(RpcError(QJsonValue(), -32700, "MCP message exceeds 1 MiB"));
                continue;
            }
            QJsonParseError parse;
            const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(line), &parse);
            if (parse.error != QJsonParseError::NoError || !document.isObject()) {
                WriteMessage(RpcError(QJsonValue(), -32700, "Invalid JSON-RPC JSON"));
                continue;
            }
            bool hasResponse = false;
            QJsonObject response;
            try { response = server.Handle(document.object(), &hasResponse); }
            catch (const std::exception &e) { response = RpcError(document.object().value("id"), -32603, QString::fromUtf8(e.what())); hasResponse = document.object().contains("id"); }
            if (hasResponse) WriteMessage(response);
        }
    } catch (const std::exception &e) {
        std::cerr << "MCP startup failed: " << e.what() << '\n'; return 2;
    }
    return 0;
}
