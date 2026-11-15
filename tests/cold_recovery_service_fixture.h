#ifndef GETDAQ_COLD_RECOVERY_SERVICE_FIXTURE_H
#define GETDAQ_COLD_RECOVERY_SERVICE_FIXTURE_H
#include "coldhandoffprotocol.h"
#include <sys/prctl.h>
// This header follows the process helpers in ecc_network_service_test.cpp.
class ColdRecoveryFixture {
    std::string executable,config,root,token,credential;
    unsigned int generation=0,counter=0;
    std::map<std::string,GetDAQ::ColdHandoffRequest> submits;
public:
    int pid=-1;
    unsigned int port;
    GetDAQ::ColdHandoffIdentity identity;
    std::string audit;
    GetDAQ::SupervisorHttpResponse Http(const std::string &method,const std::string &path,const std::string &body="",bool authenticated=true) const {
        IO::Fd socket(ConnectPort(port));
        SendBytes(socket.value,method+" "+path+" HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: "+std::to_string(body.size())+"\r\nContent-Type: application/json\r\n"+(authenticated ? "Authorization: Bearer "+token+"\r\n" : "")+"Connection: close\r\n\r\n"+body);
        const auto response=ReadHttp(socket.value);GetDAQ::SupervisorHttpResponse r;r.code=unsigned(std::stoul(response.substr(9,3)));r.body=response.substr(response.find("\r\n\r\n")+4);return r;
    }
    ColdRecoveryFixture(const std::string &service,const std::string &tool,const std::string &oldConfig,const std::string &newConfig,const std::string &plan,const std::string &original,const std::string &directory)
        : executable(service),root(directory),token(IO::RandomSession()),credential(root+"/recovery-credential"),port(Port()),audit(root+"/recovery-tasks") {
        IO::CreateDirectory(audit);IO::WriteNew(credential,token+"\n");config=root+"/recovery-service.json";
        IO::WriteNew(config,"{\"schema\":1,\"kind\":\"getdaq.cold-handoff-service\",\"service_id\":\"test-recovery\",\"bind\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+",\"executable\":"+EncodeJsonString(tool)+",\"executable_sha256\":"+EncodeJsonString(ConfigurationSha256(IO::ReadFile(tool,64*1024*1024)))+",\"old_config\":"+EncodeJsonString(oldConfig)+",\"new_config\":"+EncodeJsonString(newConfig)+",\"successor_plan\":"+EncodeJsonString(plan)+",\"original_evidence\":"+EncodeJsonString(original)+",\"audit_root\":"+EncodeJsonString(audit)+"}\n");
        Start();Require(Http("GET","/v1/cold-handoff/identity","",false).code==401,"Unauthenticated recovery identity exposed");
        Require(Http("POST","/v1/cold-handoff/tasks","{}",false).code==401,"Unauthenticated recovery submit accepted");
        Require(Http("POST","/v1/operations","{}").code==404,"Recovery exposed arbitrary ECC operation route");
        Require(Http("POST","/v1/cold-handoff/tasks","{\"schema\":1,\"schema\":1}").code==400,"Recovery accepted duplicate JSON fields");
        Require(Http("POST","/v1/cold-handoff/tasks","{\"command\":\"prepare\",\"path\":\"/tmp/foreign\"}").code==400,"Recovery accepted arbitrary command/path fields");
    }
    ~ColdRecoveryFixture(){try{Stop();}catch(...){if(pid>0){::kill(pid,SIGKILL);::waitpid(pid,nullptr,0);}}}
    void Start() {
        IO::Fd log(::open((root+"/recovery-service-"+std::to_string(generation++)+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));Require(log.value>=0,"Recovery fixture log");
        posix_spawn_file_actions_t actions;Require(!::posix_spawn_file_actions_init(&actions),"Recovery spawn actions");
        ::posix_spawn_file_actions_adddup2(&actions,log.value,1);::posix_spawn_file_actions_adddup2(&actions,log.value,2);
        std::vector<std::string> words={executable,"--config",config};std::vector<char *> args;for(auto &w:words)args.push_back(&w[0]);args.push_back(nullptr);pid_t child=0;
        const int result=::posix_spawn(&child,executable.c_str(),&actions,nullptr,args.data(),environ);::posix_spawn_file_actions_destroy(&actions);Require(!result,"Spawn recovery service");pid=child;
        Wait([&]{int status=0;const auto ended=::waitpid(pid,&status,WNOHANG);if(ended==pid)pid=-1;Require(!ended,"Recovery startup exited");try{return Http("GET","/v1/cold-handoff/identity").code==200;}catch(...){return false;}});
        identity=GetDAQ::DecodeColdHandoffIdentity(Http("GET","/v1/cold-handoff/identity").body);
    }
    void Stop(){if(pid>0){Require(!::kill(pid,SIGTERM),"Signal recovery service");int status=0;Require(::waitpid(pid,&status,0)==pid && WIFEXITED(status) && !WEXITSTATUS(status),"Recovery normal exit");pid=-1;}}
    std::string Attempt(const std::string &id) const{return audit+"/attempt-"+id;}
    void Call(const std::vector<std::string> &arguments,const std::string &logFile,bool success=true) {
        const bool submit=arguments.at(0)=="cold-complete-submit";GetDAQ::ColdHandoffRequest request;
        if(submit && submits.count(arguments.at(5)))request=submits.at(arguments[5]);
        else {
            request.taskId=submit ? arguments.at(5) : "inspect-"+std::to_string(++counter);request.instanceId=identity.instanceId;request.policySha256=identity.policySha256;request.action=submit ? "submit" : "inspect";request.attemptId=arguments.at(5);
            if(submit)submits.emplace(request.taskId,request);
        }
        const auto response=Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(request));
        if(response.code!=202){IO::WriteNew(logFile,response.body);Require(!success,"Recovery task unexpectedly rejected");return;}
        auto operation=GetDAQ::DecodeColdHandoffOperation(response.body);
        Wait([&]{operation=GetDAQ::DecodeColdHandoffOperation(Http("GET","/v1/cold-handoff/tasks/"+request.taskId).body);return operation.phase=="complete" || operation.phase=="failed" || operation.phase=="unknown";},180000);
        std::string diagnostic=GetDAQ::EncodeColdHandoffOperation(operation);const auto log=audit+"/job-"+request.taskId+"/diagnostic.log";
        if(!::access(log.c_str(),F_OK))diagnostic+=IO::ReadFile(log,65536);
        IO::WriteNew(logFile,diagnostic);
        Require((operation.phase=="complete")==success,("Recovery task result differs: "+diagnostic).c_str());
        auto changed=request;changed.action=request.action=="submit" ? "inspect" : "submit";changed.attemptId=changed.taskId;
        Require(Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(changed)).code==409,"Recovery reused task with different intent");
    }
    std::string Window(const std::string &probe,const std::string &mode,const std::string &attempt="") {
        const auto output=root+"/cold-window-"+mode+".json";
        std::vector<std::string> arguments={std::to_string(port),credential,"test-recovery",output,mode};if(!attempt.empty())arguments.push_back(attempt);
        HandoffTool(probe,arguments,output+".log");
        const auto proof=ParseStrictJson(IO::ReadFile(output,1024*1024),[] {JsonLimits limits;limits.stringBytes=512*1024;return limits;}());Require(IO::Boolean(proof.At("passed")),"Production cold window failed");
        const auto operation=GetDAQ::DecodeColdHandoffOperation(IO::String(proof.At("operation")));
        if(mode=="submit")submits.emplace(operation.request.taskId,operation.request);
        return operation.request.attemptId;
    }
    void CrashAndInspect(const std::string &marker,const std::string &gate) {
        Require(!::prctl(PR_SET_CHILD_SUBREAPER,1),"Own orphaned native child for crash test cleanup");
        GetDAQ::ColdHandoffRequest request;request.taskId=request.attemptId="crash-original";request.instanceId=identity.instanceId;request.policySha256=identity.policySha256;request.action="submit";
        Require(Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(request)).code==202,"Accept crash-bound task");
        Wait([&]{return !::access(marker.c_str(),F_OK) && !IO::ReadFile(marker,32).empty();});const int child=std::stoi(IO::ReadFile(marker,32));Require(child>0 && !::kill(child,0),"Positively live gated child");
        Require(!::kill(pid,SIGKILL) && ::waitpid(pid,nullptr,0)==pid,"Crash actual recovery service");pid=-1;Start();
        auto original=GetDAQ::DecodeColdHandoffOperation(Http("GET","/v1/cold-handoff/tasks/"+request.taskId).body);Require(original.phase=="unknown","Restart silently completed/replayed unfinished task");
        Require(Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(request)).code==202,"Observe original saved request after crash");
        auto next=request;next.taskId=next.attemptId="must-not-replay";next.instanceId=identity.instanceId;
        Require(Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(next)).code==409,"Unknown original allowed a new submit");
        Require(!::access(marker.c_str(),F_OK) && ::access(Attempt(next.taskId).c_str(),F_OK)!=0,"Refusal created native attempt");
        Require(!::unlink(gate.c_str()),"Release owned live native child");int exit=0;
        Wait([&]{const auto reaped=::waitpid(child,&exit,WNOHANG);Require(reaped>=0,"Reap owned orphan");return reaped==child;},180000);
        Require(WIFEXITED(exit) && !WEXITSTATUS(exit),"Actual orphaned native CLI did not complete");
        std::vector<std::string> inspect={"cold-complete-inspect","","","","",request.attemptId,Attempt(request.attemptId)};Call(inspect,root+"/crash-inspect.log");
        original=GetDAQ::DecodeColdHandoffOperation(Http("GET","/v1/cold-handoff/tasks/"+request.taskId).body);Require(original.phase=="unknown","Read-only recovery rewrote original unknown task");
        IO::WriteNew(root+"/crash-result.json","{\"passed\":true,\"real_service_sigkill\":true,\"live_child_preserved\":true,\"restart_unknown_no_replay\":true,\"new_submit_held\":true,\"explicit_inspect_proved_original\":true,\"original_unknown_preserved\":true}\n");
    }
    void RestartAndCheck(const std::string &attempt) {
        const auto originalInstance=identity.instanceId;const auto proof=IO::ReadFile(Attempt(attempt)+"/receipt.json",256*1024);
        Stop();Start();Require(identity.instanceId!=originalInstance,"Recovery restart retained instance identity");
        const auto request=submits.at(attempt);const auto replay=Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(request));
        Require(replay.code==202 && GetDAQ::DecodeColdHandoffOperation(replay.body).phase=="complete","Original request was not idempotently observable after restart");
        Require(IO::ReadFile(Attempt(attempt)+"/receipt.json",256*1024)==proof,"Recovery restart replay changed receipt");
        auto stale=request;stale.taskId=stale.attemptId="stale-new-submit";
        Require(Http("POST","/v1/cold-handoff/tasks",GetDAQ::EncodeColdHandoffRequest(stale)).code==409,"Stale service instance authorized a new task");
        IO::WriteNew(root+"/recovery-service-result.json","{\"passed\":true,\"authentication_required\":true,\"restart_new_instance\":true,\"old_request_observed_without_replay\":true,\"different_request_same_id_refused\":true,\"stale_instance_refused\":true,\"raw_in_qt\":false}\n");
    }
};
#endif
