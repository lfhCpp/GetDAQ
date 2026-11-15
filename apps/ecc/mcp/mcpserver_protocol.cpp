#include "mcpserver_p.h"
#include "mcptoolschema.h"
#include "mcpstdio.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>

namespace GetDAQControlMcpInternal {
using namespace GetDAQControlMcpStdio;
namespace {
const char *ProtocolVersion = "2025-11-25";
QJsonObject ToolResult(const QString &text, bool error = false) {
    QJsonObject item; item.insert("type", "text"); item.insert("text", text);
    QJsonArray content; content.append(item);
    QJsonObject result; result.insert("content", content); result.insert("isError", error);
    return result;
}
}

QJsonObject Server::Handle(const QJsonObject &request, bool *hasResponse)
{
    const QJsonValue id = request.value("id");
    *hasResponse = request.contains("id");
    if (request.value("jsonrpc").toString() != "2.0" || !request.value("method").isString())
        return RpcError(id, -32600, "Invalid JSON-RPC request");
    const QString method = request.value("method").toString();
    const QJsonObject params = request.value("params").toObject();
    if (method == "initialize") {
        if (!*hasResponse) return RpcError(id, -32600, "initialize must be a request");
        fInitialized = false;
        const QString requestedVersion = params.value("protocolVersion").toString();
        const QString negotiatedVersion = requestedVersion == ProtocolVersion ? requestedVersion : ProtocolVersion;
        fInitializePending = true;
        QJsonObject capabilities; capabilities.insert("tools", QJsonObject());
        QJsonObject serverInfo; serverInfo.insert("name", "getdaq-control-mcp"); serverInfo.insert("version", "1.0.0");
        QJsonObject result; result.insert("protocolVersion", negotiatedVersion); result.insert("capabilities", capabilities); result.insert("serverInfo", serverInfo);
        QJsonObject response; response.insert("jsonrpc", "2.0"); response.insert("id", id); response.insert("result", result);
        return response;
    }
    if (method == "notifications/initialized") {
        if (fInitializePending) fInitialized = true;
        *hasResponse = false; return {};
    }
    if (method == "notifications/cancelled") { *hasResponse = false; return {}; }
    if (method == "ping") {
        QJsonObject response; response.insert("jsonrpc", "2.0"); response.insert("id", id); response.insert("result", QJsonObject()); return response;
    }
    if (!fInitialized) return RpcError(id, -32002, "Send initialize and notifications/initialized first");
    if (method == "tools/list") {
        QJsonArray tools; tools.append(GetDAQControlMcpToolSchema::ReadTool("ecc_identity", "Read fixed ECC service identity and snapshot binding."));
        tools.append(GetDAQControlMcpToolSchema::ReadTool("ecc_status", "Read the cached ECC, operation and data-node status. This does not wait for hardware or read raw data."));
        tools.append(GetDAQControlMcpToolSchema::ReadTool("ecc_cobo_lifecycles", "Read fixed active CoBo lifecycle projections and their latest operation evidence. Derived from ECC lifecycle/journal, not hardware readback; service restart or uncertain history may show UNKNOWN. Read-only metadata; does not wait for hardware or perform recovery."));
        tools.append(GetDAQControlMcpToolSchema::ReadTool("ecc_hardware_state", "Perform one bounded fresh read of fixed CoBo/MuTaNT control bits and return cached per-CoBo lifecycle phases. Lifecycle is coordination cache, not hardware readback. Read-only; no recovery or writes."));
        tools.append(GetDAQControlMcpToolSchema::ReadTool("ecc_receivers", "Read the service's cached observation of its receiver processes."));
        tools.append(GetDAQControlMcpToolSchema::OperationTool());
        if (fSettings.allowControl) { tools.append(GetDAQControlMcpToolSchema::ControlTool()); tools.append(GetDAQControlMcpToolSchema::ReprepareCoBoTool()); tools.append(GetDAQControlMcpToolSchema::ConfigureCoBoTool()); tools.append(GetDAQControlMcpToolSchema::CatalogTool()); tools.append(GetDAQControlMcpToolSchema::CatalogRecoveryTool()); }
        QJsonObject result; result.insert("tools", tools);
        QJsonObject response; response.insert("jsonrpc", "2.0"); response.insert("id", id); response.insert("result", result); return response;
    }
    if (method == "tools/call") {
        if (!*hasResponse) return RpcError(id, -32600, "tools/call must be a request");
        const QString name = params.value("name").toString();
        const QJsonObject arguments = params.value("arguments").toObject();
        QJsonObject response; response.insert("jsonrpc", "2.0"); response.insert("id", id);
        try { response.insert("result", ToolResult(Call(name, arguments))); }
        catch (const std::exception &e) { response.insert("result", ToolResult(QString::fromUtf8(e.what()), true)); }
        return response;
    }
    if (!*hasResponse) { *hasResponse = false; return {}; }
    return RpcError(id, -32601, "Method not found");
}

} // namespace GetDAQControlMcpInternal
