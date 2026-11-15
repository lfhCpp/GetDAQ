#include "mcptoolschema.h"

#include <QJsonArray>
#include <QStringList>

namespace GetDAQControlMcpToolSchema {

QJsonObject ReadTool(const QString &name, const QString &description)
    {
        QJsonObject schema; schema.insert("type", "object"); schema.insert("properties", QJsonObject()); schema.insert("additionalProperties", false);
        QJsonObject annotations; annotations.insert("readOnlyHint", true); annotations.insert("openWorldHint", false);
        QJsonObject tool; tool.insert("name", name); tool.insert("description", description);
        tool.insert("inputSchema", schema); tool.insert("annotations", annotations); return tool;
    }

QJsonObject OperationTool()
    {
        QJsonObject operation;
        operation.insert("type", "string");
        operation.insert("pattern", "^[A-Za-z0-9_-]{1,128}$");
        QJsonObject properties; properties.insert("operation_id", operation);
        QJsonObject schema; schema.insert("type", "object"); schema.insert("properties", properties);
        schema.insert("required", QJsonArray({"operation_id"})); schema.insert("additionalProperties", false);
        QJsonObject tool; tool.insert("name", "ecc_operation");
        tool.insert("description", "Read cached state for one exact operation_id, including request, phase, message and result. You can query the operation_id returned by ecc_cobo_lifecycles. After an uncertain submission, query that same ID; this tool does not retry or change hardware state.");
        QJsonObject annotations; annotations.insert("readOnlyHint", true); annotations.insert("openWorldHint", false);
        tool.insert("inputSchema", schema); tool.insert("annotations",annotations); return tool;
    }

QJsonObject ControlTool()
    {
        QJsonObject properties;
        QJsonObject command; command.insert("type", "string"); QJsonArray commands;
        const QStringList allowedCommands = QStringList() << "describe" << "prepare" << "configure" << "start"
            << "stop" << "pause" << "resume" << "breakup" << "reset";
        for (const QString &value : allowedCommands) commands.append(value);
        command.insert("enum", commands); command.insert("description", "A lifecycle command; the fixed ECC service enforces authoritative state and recovery gates.");
        properties.insert("command", command);
        QJsonObject operation; operation.insert("type", "string"); operation.insert("pattern", "^[A-Za-z0-9_-]{1,128}$"); operation.insert("description", "New stable id for this intent; reuse the same ID only to observe/retry the exact same intent."); properties.insert("operation_id", operation);
        QJsonObject confirmation; confirmation.insert("type", "string"); confirmation.insert("description", "Must exactly equal I_CONFIRM_<command>_<operation_id>; MCP host should obtain human approval before this tool call."); properties.insert("confirmation", confirmation);
        QJsonObject schema; schema.insert("type", "object"); schema.insert("properties", properties); schema.insert("required", QJsonArray({"command", "operation_id", "confirmation"})); schema.insert("additionalProperties", false);
        QJsonObject annotations; annotations.insert("readOnlyHint", false); annotations.insert("destructiveHint", true); annotations.insert("idempotentHint", false); annotations.insert("openWorldHint", false);
        QJsonObject tool; tool.insert("name", "ecc_submit_operation"); tool.insert("description", "Submit one explicit native ECC lifecycle operation. Disabled unless allow_control is true in the local MCP config. Requires a host-approved confirmation string and a unique operation ID. After a timeout or uncertain response, query that ID instead of blindly resubmitting. Never uploads raw data."); tool.insert("inputSchema", schema); tool.insert("annotations", annotations); return tool;
    }

QJsonObject CatalogTool()
    {
        QJsonObject properties;
        QJsonObject experiment; experiment.insert("type", "string"); experiment.insert("pattern", "^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$");
        experiment.insert("description", "Experiment name for the fixed sealed receiver sessions."); properties.insert("experiment", experiment);
        QJsonObject run; run.insert("type", "integer"); run.insert("minimum", 1); run.insert("maximum", 2147483646);
        run.insert("description", "Explicit formal Run number; the ECC service rejects incomplete or mismatched receiver cycles."); properties.insert("run_number", run);
        QJsonObject replace; replace.insert("type", "boolean");
        replace.insert("description", "Set true only after explicit operator approval to replace this completed Run. Only matching raw names are replaced; other old raw files are retained."); properties.insert("replace_completed_run", replace);
        QJsonObject operation; operation.insert("type", "string"); operation.insert("pattern", "^[A-Za-z0-9_-]{1,128}$"); properties.insert("operation_id", operation);
        QJsonObject confirmation; confirmation.insert("type", "string");
        confirmation.insert("description", "Must equal I_CONFIRM_catalog_<operation_id>; MCP host must obtain human approval before the call."); properties.insert("confirmation", confirmation);
        QJsonObject schema; schema.insert("type", "object"); schema.insert("properties", properties);
        schema.insert("required", QJsonArray({"experiment", "run_number", "replace_completed_run", "operation_id", "confirmation"}));
        schema.insert("additionalProperties", false);
        QJsonObject annotations; annotations.insert("readOnlyHint", false); annotations.insert("destructiveHint", true);
        annotations.insert("idempotentHint", false); annotations.insert("openWorldHint", false);
        QJsonObject tool; tool.insert("name", "ecc_catalog_run");
        tool.insert("description", "Catalog the current normally stopped and sealed receiver sessions under an explicit Experiment/Run. Disabled unless allow_control is true. Setting replacement true requires operator approval; the ECC state machine and per-node Prepare barrier remain authoritative. Never transfers raw data through MCP.");
        tool.insert("inputSchema", schema); tool.insert("annotations", annotations); return tool;
    }

QJsonObject ConfigureCoBoTool()
    {
        QJsonObject properties;
        QJsonObject target; target.insert("type","string");target.insert("pattern","^[A-Za-z0-9_-]{1,128}$");
        target.insert("description","Exact fixed active CoBo ID reviewed by the operator.");properties.insert("cobo_id",target);
        QJsonObject operation;operation.insert("type","string");operation.insert("pattern","^[A-Za-z0-9_-]{1,128}$");properties.insert("operation_id",operation);
        QJsonObject confirmation;confirmation.insert("type","string");confirmation.insert("description","Must equal I_CONFIRM_configure-cobo_<cobo_id>_<operation_id>; MCP host must obtain explicit operator approval.");properties.insert("confirmation",confirmation);
        QJsonObject schema;schema.insert("type","object");schema.insert("properties",properties);schema.insert("required",QJsonArray({"cobo_id","operation_id","confirmation"}));schema.insert("additionalProperties",false);
        QJsonObject annotations;annotations.insert("readOnlyHint",false);annotations.insert("destructiveHint",true);annotations.insert("idempotentHint",false);annotations.insert("openWorldHint",false);
        QJsonObject tool;tool.insert("name","ecc_configure_cobo");tool.insert("description","Configure exactly one fixed active CoBo using the existing Prepared deployment plan. Requires healthy Prepared, stopped data processes and explicit operator approval. Other CoBos and MuTaNT are untouched; the system remains Prepared and no Run starts. Query the same operation ID after uncertain replies; never blindly resubmit. Disabled unless allow_control is true.");tool.insert("inputSchema",schema);tool.insert("annotations",annotations);return tool;
    }

QJsonObject ReprepareCoBoTool()
    {
        QJsonObject properties;
        QJsonObject target;target.insert("type","string");target.insert("pattern","^[A-Za-z0-9_-]{1,128}$");
        target.insert("description","Exact active CoBo ID from the fixed ECC configuration.");properties.insert("cobo_id",target);
        QJsonObject operation;operation.insert("type","string");operation.insert("pattern","^[A-Za-z0-9_-]{1,128}$");properties.insert("operation_id",operation);
        QJsonObject confirmation;confirmation.insert("type","string");
        confirmation.insert("description","Must equal I_CONFIRM_reprepare-cobo_<cobo_id>_<operation_id>; MCP host must obtain explicit operator approval.");properties.insert("confirmation",confirmation);
        QJsonObject schema;schema.insert("type","object");schema.insert("properties",properties);schema.insert("required",QJsonArray({"cobo_id","operation_id","confirmation"}));schema.insert("additionalProperties",false);
        QJsonObject annotations;annotations.insert("readOnlyHint",false);annotations.insert("destructiveHint",true);annotations.insert("idempotentHint",false);annotations.insert("openWorldHint",false);
        QJsonObject tool;tool.insert("name","ecc_reprepare_cobo");
        tool.insert("description","Reprepare exactly one fixed active CoBo in a healthy Prepared system, using its original immutable plan. The selected board must have known Prepared or Configured lifecycle evidence; all data processes must be stopped. Other CoBos, MuTaNT, system alignment and Run state are not advanced. Requires explicit operator approval. Query the same operation ID after an uncertain reply; never blindly resubmit. Disabled unless allow_control is true.");
        tool.insert("inputSchema",schema);tool.insert("annotations",annotations);return tool;
    }

QJsonObject CatalogRecoveryTool()
    {
        QJsonObject properties;
        QJsonObject failed; failed.insert("type", "string"); failed.insert("pattern", "^[A-Za-z0-9_-]{1,128}$");
        failed.insert("description", "Operation ID of the exact failed catalog request that explicitly replaced an existing completed Run."); properties.insert("failed_operation_id", failed);
        QJsonObject operation; operation.insert("type", "string"); operation.insert("pattern", "^[A-Za-z0-9_-]{1,128}$"); properties.insert("operation_id", operation);
        QJsonObject confirmation; confirmation.insert("type", "string");
        confirmation.insert("description", "Must equal I_CONFIRM_catalog_recovery_<failed_operation_id>_<operation_id>; obtain explicit operator approval before calling."); properties.insert("confirmation", confirmation);
        QJsonObject schema; schema.insert("type", "object"); schema.insert("properties", properties);
        schema.insert("required", QJsonArray({"failed_operation_id", "operation_id", "confirmation"})); schema.insert("additionalProperties", false);
        QJsonObject annotations; annotations.insert("readOnlyHint", false); annotations.insert("destructiveHint", true);
        annotations.insert("idempotentHint", false); annotations.insert("openWorldHint", false);
        QJsonObject tool; tool.insert("name", "ecc_resume_catalog_replacement");
        tool.insert("description", "Explicitly resume one failed Run replacement Publish using only retained evidence from the same live ECC/data-supervisor processes. Does not work after service restart. The ECC service verifies the original failed replacement, known Prepared controller and node receipts; never replays an uncertain node request or transfers raw data through MCP.");
        tool.insert("inputSchema", schema); tool.insert("annotations", annotations); return tool;
    }

}
