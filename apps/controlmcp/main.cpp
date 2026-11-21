// MCP stdio adapter for the same original SOAP ECC/Sentry clients as the GUI.
// No board transport, shell execution, backend start/stop, or raw-data path.
#include "runconfig.h"
#include "eccclient.h"
#include "sentryclient.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFile>
#include <QSaveFile>
#include <QLockFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QDateTime>
#include <iostream>
#include <stdexcept>
#include <csignal>

namespace {
struct RpcFailure:std::runtime_error {int code;RpcFailure(int c,const QString &s):std::runtime_error(s.toStdString()),code(c){}};
void Need(bool ok,const QString &message){if(!ok)throw RpcFailure(-32602,message);}
QByteArray Json(const QJsonObject &o){return QJsonDocument(o).toJson(QJsonDocument::Compact);}
QString Digest(const QByteArray &s){return QString::fromLatin1(QCryptographicHash::hash(s,QCryptographicHash::Sha256).toHex());}
QJsonObject Read(const QString &path){QFile f(path);Need(f.open(QIODevice::ReadOnly),"Cannot open JSON file: "+path);Need(f.size()<=1024*1024,"JSON file too large");QJsonParseError e;auto d=QJsonDocument::fromJson(f.readAll(),&e);Need(e.error==QJsonParseError::NoError && d.isObject(),"Invalid JSON file: "+path);return d.object();}
void Write(const QString &path,const QJsonObject &o){QSaveFile f(path);Need(f.open(QIODevice::WriteOnly),"Cannot persist MCP operation");f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);auto bytes=Json(o);Need(f.write(bytes)==bytes.size() && f.commit(),"Cannot commit MCP operation");}
QJsonObject ObjectSchema(const QJsonObject &properties={},const QJsonArray &required={}){return {{"type","object"},{"properties",properties},{"required",required},{"additionalProperties",false}};}
QJsonObject Tool(const QString &name,const QString &description,const QJsonObject &schema,bool readOnly){return {{"name",name},{"description",description},{"inputSchema",schema},{"annotations",QJsonObject{{"readOnlyHint",readOnly},{"destructiveHint",!readOnly},{"openWorldHint",true},{"idempotentHint",readOnly}}}};}
QJsonObject TextResult(const QJsonObject &o,bool error=false){return {{"content",QJsonArray{QJsonObject{{"type","text"},{"text",QString::fromUtf8(Json(o))}}}},{"structuredContent",o},{"isError",error}};}
void Fields(const QJsonObject &o,const QStringList &allowed){for(auto it=o.begin();it!=o.end();++it)Need(allowed.contains(it.key()),"Unknown argument: "+it.key());}
const QJsonObject stringType{{"type","string"}};
const QJsonObject nodeType{{"type","integer"},{"minimum",0}};

class Adapter {
 RunConfig config;QString fingerprint,operations;bool allow=false,initialized=false,ready=false;int timeout=30000;
public:
 explicit Adapter(const QString &path){
  const auto options=Read(path);Fields(options,{"schema","run_config","allow_control","request_timeout_ms","operation_directory"});
  Need(options["run_config"].isString(),"Set run_config to the GUI's saved envoy.yml. The retired native base_url/credential configuration is not supported.");
  const auto yaml=QDir(QFileInfo(path).absolutePath()).absoluteFilePath(options["run_config"].toString());QString error;Need(config.Load(yaml,&error),error);QFile f(yaml);Need(f.open(QIODevice::ReadOnly),"Cannot read Run configuration");fingerprint=Digest(f.readAll());
  if(options.contains("allow_control"))Need(options["allow_control"].isBool(),"allow_control must be boolean");allow=options["allow_control"].toBool();
  if(options.contains("request_timeout_ms")){const auto value=options["request_timeout_ms"];Need(value.isDouble() && value.toDouble()==value.toInt() && value.toInt()>=100 && value.toInt()<=120000,"request_timeout_ms must be an integer from 100 to 120000");timeout=value.toInt();}
  operations=options["operation_directory"].toString();if(operations.isEmpty())operations=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/operations/"+Digest(QFileInfo(yaml).absoluteFilePath().toUtf8());else operations=QDir(QFileInfo(path).absolutePath()).absoluteFilePath(operations);
 }
 QJsonObject Ecc(const NodeConfig &node,const QString &operation,int budget){
  auto c=config;c.timeoutMs=c.stepTimeoutMs=qMax(100,budget);EccClient client(node,c);QEventLoop loop;QJsonObject result;
  QObject::connect(&client,&EccClient::ReplySignal,&loop,[&](int,QString,int state,int transition,QString error){result={{"node_id",node.id},{"host",node.host},{"port",node.eccPort},{"state",state},{"state_name",StateName(state)},{"transition",transition},{"error",error},{"outcome_unknown",operation!="GetState" && !error.isEmpty()}};loop.quit();});
  Need(client.Send(operation),"ECC request could not be sent");loop.exec();return result;
 }
 const NodeConfig &Node(const QJsonValue &id){Need(id.isDouble() && id.toDouble()==id.toInt(),"Supply an integer node_id from ecc_configuration");for(const auto &n:config.nodes)if(n.id==id.toInt())return n;throw RpcFailure(-32602,"Unknown node_id");}
 QJsonArray Tools(){
  QJsonArray tools{Tool("ecc_status","Query actual original SOAP ECC states; optional node_id selects one endpoint.",ObjectSchema({{"node_id",nodeType}}),true),Tool("ecc_receivers","Query Sentry observations for original DataExporter or external DataRouter. No raw files are read.",ObjectSchema({{"node_id",nodeType}}),true),Tool("ecc_configuration","Show the saved GUI YAML's endpoints, configuration IDs and path meanings.",ObjectSchema(),true),Tool("ecc_operation","Inspect this adapter's durable command receipt. Unknown outcomes are never replayed.",ObjectSchema({{"operation_id",stringType}},{"operation_id"}),true)};
  if(allow)tools<<Tool("ecc_submit_operation","Send one original SOAP command to one node. Requires operator confirmation I_CONFIRM_<command>_<operation_id>. GUI Run records/catalog are separate. A command acknowledgment does not prove the target state; inspect ecc_status.",ObjectSchema({{"node_id",nodeType},{"command",QJsonObject{{"type","string"},{"enum",QJsonArray{"describe","prepare","configure","start","stop","pause","resume","undo","breakup"}}}},{"operation_id",stringType},{"confirmation",stringType}},{"node_id","command","operation_id","confirmation"}),false);
  return tools;
 }
 QString OperationPath(const QJsonValue &id){Need(id.isString() && QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(id.toString()).hasMatch(),"Invalid operation_id (1-80 letters/digits/_/-)");return QDir(operations).filePath(id.toString()+".json");}
 QJsonObject Receipt(const QString &path){auto record=Read(path);if(record["phase"]=="accepted"){record["phase"]="unknown";record["message"]="Adapter interrupted after intent was saved. Inspect ECC state; this operation will not be replayed.";}return record;}
 QJsonObject Call(const QString &name,const QJsonObject &args){
  if(name=="ecc_configuration"){
   Fields(args,{});QJsonArray nodes;for(const auto &n:config.nodes)nodes<<QJsonObject{{"node_id",n.id},{"name",n.name},{"ecc_host",n.host},{"soap_port",n.eccPort},{"router_host",n.routerHost.isEmpty()?n.host:n.routerHost},{"router_data_port",n.routerPort},{"export_host",n.exporterHost.isEmpty()?n.host:n.exporterHost},{"export_port",n.exporterPort},{"describe_id",n.ConfigId("describe",config.experiment)},{"prepare_id",n.ConfigId("prepare",config.experiment)},{"configure_id",n.ConfigId("configure",config.experiment)}};
   return TextResult({{"experiment",config.experiment},{"nodes",nodes},{"configuration_source_or_mirror",config.configDir},{"gui_run_records_root",config.outputDir},{"raw_directory","Query ecc_receivers data_path on the actual receiver host"},{"allow_control",allow},{"operation_directory",operations},{"configuration_sha256",fingerprint},{"raw_data_read",false}});
  }
  if(name=="ecc_operation"){Fields(args,{"operation_id"});return TextResult(Receipt(OperationPath(args["operation_id"])));}
  if(name=="ecc_status" || name=="ecc_receivers"){
   Fields(args,{"node_id"});QVector<NodeConfig> selected=config.nodes;if(args.contains("node_id"))selected={Node(args["node_id"])};QJsonArray items;QElapsedTimer timer;timer.start();bool failed=false;
   for(const auto &n:selected){if(name=="ecc_receivers" && n.mutant)continue;const int budget=timeout-int(timer.elapsed());QJsonObject result;
    if(budget<100)result={{"node_id",n.id},{"error","Observation time budget exhausted; query this node separately"}};
    else if(name=="ecc_status")result=Ecc(n,"GetState",qMin(config.timeoutMs,budget));
    else{auto c=config;c.timeoutMs=budget;SentryClient client(n,c);QEventLoop loop;QObject::connect(&client,&SentryClient::ReplySignal,&loop,[&](int,bool,NodeStatus s,QString e){result={{"node_id",n.id},{"data_path",s.dataPath},{"files",s.files},{"written_gb",s.writtenGB},{"available_gb",s.availableGB},{"total_gb",s.totalGB},{"error",e}};loop.quit();});Need(client.Send(),"Cannot start receiver observation");loop.exec();}
    failed|=!result["error"].toString().isEmpty();items<<result;
   }return TextResult({{"items",items},{"raw_data_read",false}},failed);
  }
  if(name=="ecc_submit_operation"){
   Need(allow,"Control disabled. Set allow_control=true in the local MCP configuration to expose command submission.");Fields(args,{"node_id","command","operation_id","confirmation"});const auto &node=Node(args["node_id"]);Need(args["command"].isString(),"command must be a string");const auto command=args["command"].toString();const QMap<QString,QString> verbs{{"describe","Describe"},{"prepare","Prepare"},{"configure","Configure"},{"start","Start"},{"stop","Stop"},{"pause","Pause"},{"resume","Resume"},{"undo","Undo"},{"breakup","Breakup"}};Need(verbs.contains(command),"Unsupported original ECC command");const auto path=OperationPath(args["operation_id"]);
   Need(args["confirmation"].toString()=="I_CONFIRM_"+command+'_'+args["operation_id"].toString(),"Explicit operator confirmation required: I_CONFIRM_<command>_<operation_id>");
   Need(QDir().mkpath(operations),"Cannot create MCP operation directory");QFile::setPermissions(operations,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);QLockFile commandLock(QDir(operations).filePath("command.lock"));Need(commandLock.tryLock(0),"Another MCP command is in progress for this operation directory; query state before continuing");
   const auto requestHash=Digest(Json(args)+fingerprint.toUtf8());if(QFile::exists(path)){auto previous=Receipt(path);Need(previous["request_sha256"]==requestHash,"operation_id was already used with another command/configuration");return TextResult(previous,previous["phase"]!="acknowledged");}
   Need(QDir().mkpath(operations),"Cannot create MCP operation directory");QFile::setPermissions(operations,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);Need(QDir(operations).entryList({"*.json"},QDir::Files).size()<4096,"MCP receipt budget reached; preserve existing receipts before choosing a new operation directory");
   QJsonObject record{{"operation_id",args["operation_id"]},{"request_sha256",requestHash},{"node_id",node.id},{"command",command},{"phase","accepted"},{"created_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"configuration_sha256",fingerprint}};Write(path,record);
   const auto result=Ecc(node,verbs[command],qMin(timeout,config.stepTimeoutMs));const bool error=!result["error"].toString().isEmpty();record["phase"]=error?"unknown":"acknowledged";record["result"]=result;record["message"]="A SOAP acknowledgment is separate from state confirmation. Query ecc_status. No GUI Run reservation or Sentry catalog was issued.";
   try{Write(path,record);}catch(const std::exception &e){record["receipt_warning"]=e.what();}return TextResult(record,error);
  }
  throw RpcFailure(-32602,"Unknown or unavailable MCP tool: "+name);
 }
 QJsonObject Request(const QJsonObject &request){
  Need(request["jsonrpc"]=="2.0" && request["method"].isString(),"Expected JSON-RPC 2.0 request");const auto method=request["method"].toString();if(request.contains("params"))Need(request["params"].isObject(),"params must be an object");const auto params=request["params"].toObject();
  if(method=="ping")return {};
  if(method=="initialize"){Need(!initialized,"Already initialized");Need(params["protocolVersion"].isString(),"protocolVersion required");initialized=true;const auto requested=params["protocolVersion"].toString();const QStringList versions{"2024-11-05","2025-03-26","2025-06-18","2025-11-25"};return {{"protocolVersion",versions.contains(requested)?requested:QString("2025-11-25")},{"capabilities",QJsonObject{{"tools",QJsonObject{{"listChanged",false}}}}},{"serverInfo",QJsonObject{{"name","getdaq-control-mcp"},{"version","2.0.0"}}},{"instructions","Original SOAP ECC adapter. Reuse the GUI YAML. Observation only by default; explicit commands do not manage GUI Run journals or raw data."}};}
  if(!initialized || !ready)throw RpcFailure(-32002,"Initialize MCP and send notifications/initialized first");
  if(method=="tools/list"){Need(!params.contains("cursor"),"This server has no additional tool pages");return {{"tools",Tools()}};}
  if(method=="tools/call"){Need(params["name"].isString(),"Tool name required");Need(!params.contains("arguments") || params["arguments"].isObject(),"arguments must be an object");return Call(params["name"].toString(),params["arguments"].toObject());}
  throw RpcFailure(-32601,"Method not found");
 }
 void Notification(const QJsonObject &request){if(initialized && request["jsonrpc"]=="2.0" && request["method"]=="notifications/initialized")ready=true;}
};
}
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);app.setOrganizationName("GetDAQ");app.setApplicationName("getdaq-control-mcp");
#ifdef SIGPIPE
 std::signal(SIGPIPE,SIG_IGN);
#endif
 QCommandLineParser parser;parser.setApplicationDescription("MCP stdio for original SOAP ECC/Sentry; uses saved GUI YAML.");parser.addHelpOption();parser.addOption({"config","MCP JSON configuration containing run_config.","path"});parser.process(app);
 try{Need(parser.isSet("config"),"Supply --config /path/to/ecc-control-mcp.json");Adapter adapter(parser.value("config"));
  while(true){std::string line;char c;bool oversized=false;while(std::cin.get(c) && c!='\n'){if(line.size()<1024*1024)line+=c;else oversized=true;}if(line.empty() && !std::cin)break;QJsonValue id=QJsonValue::Null;QJsonObject response{{"jsonrpc","2.0"},{"id",id}};
   try{if(oversized)throw RpcFailure(-32600,"MCP message exceeds 1 MiB");QJsonParseError e;auto document=QJsonDocument::fromJson(QByteArray::fromStdString(line),&e);if(e.error!=QJsonParseError::NoError)throw RpcFailure(-32700,"Invalid JSON");if(!document.isObject())throw RpcFailure(-32600,"One JSON-RPC object per line is required");auto request=document.object();
    if(!request.contains("id")){adapter.Notification(request);continue;}id=request["id"];if(!(id.isString() || id.isDouble()))throw RpcFailure(-32600,"Request id must be a string or number");response["id"]=id;response["result"]=adapter.Request(request);
   }catch(const RpcFailure &e){response["error"]=QJsonObject{{"code",e.code},{"message",e.what()}};}catch(const std::exception &e){response["error"]=QJsonObject{{"code",-32603},{"message",e.what()}};}
   std::cout<<Json(response).constData()<<std::endl;
  }return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 2;}
}
