#include "receiver_catalog_fixture.h"
#include "transferjobs.h"
#include "catalogtransfer.h"
#include <chrono>
#include <thread>
#include <fstream>
#include <sys/resource.h>
#include <csignal>
using namespace GetDAQCatalogFixture;
namespace {
TransferOperation Terminal(TransferJobs &jobs,const std::string &id) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    for(;;){const auto result=jobs.Operation(id);if(result.phase=="complete" || result.phase=="failed" || result.phase=="unknown")return result;Require(std::chrono::steady_clock::now()<end,"Transfer job deadline");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
}
}
int main(int argc,char **argv) {
    try {
        if(argc==3 && std::string(argv[1])=="--hold-fixture") {
            Fixture fixture(1024*1024);auto &plan=fixture.options.receiver;
            plan.receiverExecutable=argv[2];fixture.options.executableSha256=ConfigurationSha256(ReadFile(argv[2],64*1024*1024));
            fixture.options.stopped.planSha256=ConfigurationSha256(EncodeOptions(plan,fixture.options.executableSha256));
            {ReceiverCatalog catalog(fixture.options);catalog.Execute();}
            CreateDirectory(fixture.root+"/backups");CreateDirectory(fixture.root+"/jobs");
            auto encoded=EncodeOptions(plan,fixture.options.executableSha256);while(!encoded.empty() && encoded.back()=='\n')encoded.pop_back();
            std::cout<<"{\"root\":"<<EncodeJsonString(fixture.root)<<",\"plan\":"<<encoded<<"}"<<std::endl;
            std::string done;std::getline(std::cin,done);return 0;
        }
        Require(argc==2,"Pass actual native catalog program");Fixture fixture(2*1024*1024);{ReceiverCatalog source(fixture.options);source.Execute();}
        {
            Fixture partial(131072);{ReceiverCatalog source(partial.options);source.Execute();}
            CreateDirectory(partial.root+"/backups");CreateDirectory(partial.root+"/jobs");
            TransferJobOptions o;o.agentId="resume-node";o.deploymentId=partial.options.receiver.deploymentId;o.sourceRoot=partial.options.receiver.catalogRoot;o.backupRoot=partial.root+"/backups";o.auditRoot=partial.root+"/jobs";o.executable=argv[1];o.executableSha256=ConfigurationSha256(ReadFile(argv[1],64*1024*1024));
            TransferRequest request;TransferOperation failed;
            {
                TransferJobs jobs(o);request.operationId="actual-partial";request.instanceId=jobs.Identity().instanceId;request.policySha256=jobs.Identity().policySha256;request.experiment="Test";request.sourceRequestKey=String(ParseStrictJson(jobs.SourceReceipt("Test",0)).At("request_key"));
                struct rlimit original;Require(!::getrlimit(RLIMIT_FSIZE,&original),"Get original file-size limit");auto limited=original;limited.rlim_cur=16384;const auto handler=std::signal(SIGXFSZ,SIG_IGN);
                Require(!::setrlimit(RLIMIT_FSIZE,&limited),"Limit actual copy child");
                try{jobs.Submit(request);failed=Terminal(jobs,request.operationId);}catch(...){::setrlimit(RLIMIT_FSIZE,&original);std::signal(SIGXFSZ,handler);throw;}
                Require(!::setrlimit(RLIMIT_FSIZE,&original),"Restore file-size limit");std::signal(SIGXFSZ,handler);Require(failed.phase=="failed","Actual short-write child did not fail safely");
            }
            const auto path=failed.destination+"/source_0/data0.graw";struct stat before,after;Require(!::stat(path.c_str(),&before) && before.st_size==16384,"Actual child did not leave a partial prefix");const auto oldChild=ReadFile(o.auditRoot+"/job-actual-partial/child.json",4096);
            TransferJobs reopened(o);TransferReconciliationRequest resume;resume.verificationId="explicit-resume";resume.operationId=request.operationId;resume.instanceId=reopened.Identity().instanceId;resume.policySha256=request.policySha256;resume.originalRequestKey=failed.requestKey;
            Require(DecodeTransferResumeRequest(EncodeTransferResumeRequest(resume)).originalRequestKey==failed.requestKey,"Resume request changed original binding");
            reopened.Resume(resume);const auto done=Terminal(reopened,request.operationId);Require(done.phase=="complete" && done.request.instanceId==request.instanceId && !::stat(path.c_str(),&after) && Same(before,after) && after.st_size==131072,"Native child continuation failed or replaced original partial file");
            Require(ReadFile(o.auditRoot+"/job-actual-partial/child.json",4096)==oldChild && VerifyTransferredCatalog(done.destination)==done.receipt,"Resume adopted old PID or failed complete content verification");
            const auto child=ReadFile(o.auditRoot+"/job-actual-partial/resume-explicit-resume/child.json",4096);Require(reopened.Resume(resume).phase=="complete" && ReadFile(o.auditRoot+"/job-actual-partial/resume-explicit-resume/child.json",4096)==child,"Duplicate resume launched another child");
        }
        CreateDirectory(fixture.root+"/backups");CreateDirectory(fixture.root+"/jobs");
        TransferJobOptions options;options.agentId="node-A";options.deploymentId=fixture.options.receiver.deploymentId;options.sourceRoot=fixture.options.receiver.catalogRoot;options.backupRoot=fixture.root+"/backups";options.auditRoot=fixture.root+"/jobs";options.executable=argv[1];options.executableSha256=ConfigurationSha256(ReadFile(argv[1],64*1024*1024));
        TransferRequest request;std::string destination;
        {
            TransferJobs jobs(options);const auto identity=jobs.Identity();Require(DecodeTransferIdentity(EncodeTransferIdentity(identity)).policySha256==identity.policySha256,"Transfer identity roundtrip");
            const auto receipt=ParseStrictJson(jobs.SourceReceipt("Test",0));request.operationId="test-copy";request.instanceId=identity.instanceId;request.policySha256=identity.policySha256;request.experiment="Test";request.sourceRequestKey=String(receipt.At("request_key"));
            auto wrong=request;wrong.instanceId=std::string(64,'e');Reject([&]{jobs.Submit(wrong);});wrong=request;wrong.sourceRequestKey=std::string(64,'f');Reject([&]{jobs.Submit(wrong);});
            const auto accepted=jobs.Submit(request);Require(accepted.phase=="accepted","Copy not durably accepted");destination=accepted.destination;
            Require(jobs.Submit(request).requestKey==accepted.requestKey,"Same ID duplicated child intent");
            Reject([&]{TransferJobs duplicateOwner(options);});
            const auto complete=Terminal(jobs,request.operationId);Require(complete.phase=="complete" && complete.copiedBytes==2*(2*1024*1024+4096),"Actual native child did not complete copy");
            Require(VerifyTransferredCatalog(destination)==complete.receipt,"Completed job receipt does not match independent payload verification");
            Require(DecodeTransferOperation(EncodeTransferOperation(complete)).requestKey==complete.requestKey,"Portable task protocol roundtrip");
            const auto childBefore=ReadFile(options.auditRoot+"/job-test-copy/child.json",4096);
            for(unsigned i=0;i<3;++i)Require(jobs.Submit(request).phase=="complete","Same ID resubmitted");
            Require(ReadFile(options.auditRoot+"/job-test-copy/child.json",4096)==childBefore,"Same ID launched another child");
            auto identityBad=identity;identityBad.sourceRoot+="/..";Reject([&]{EncodeTransferIdentity(identityBad);});
            auto corrupt=EncodeTransferOperation(complete);const auto marker=corrupt.find("\"content_verified\":true");Require(marker!=std::string::npos,"Receipt marker");corrupt.replace(marker,23,"\"content_verified\":\"true\"");Reject([&]{DecodeTransferOperation(corrupt);});
            wrong=request;wrong.runNumber=1;Reject([&]{jobs.Submit(wrong);});
        }
        {
            TransferJobs reopened(options);const auto original=reopened.Operation(request.operationId);Require(original.phase=="complete" && original.request.instanceId==request.instanceId && reopened.Identity().instanceId!=request.instanceId,"Completed job reopened as current instance/replayed");
            Require(reopened.Submit(request).requestKey==original.requestKey,"Original ID observation rejected after restart");
        }
        const auto jobRoot=options.auditRoot+"/job-test-copy";
        const auto original=DecodeTransferOperation(ReadFile(jobRoot+"/operation.json",128*1024));
        auto pending=original;pending.phase="running";pending.receipt.clear();
        {std::ofstream state(jobRoot+"/operation.json");state<<EncodeTransferOperation(pending);}
        const auto oldChild=ReadFile(jobRoot+"/child.json",4096);
        {
            TransferJobs reopened(options);Require(reopened.Operation(request.operationId).phase=="unknown","Unfinished history promoted/replayed");
            Require(reopened.Submit(request).phase=="unknown","Original ID replayed after unfinished restart");
            auto another=request;another.operationId="another-copy";another.instanceId=reopened.Identity().instanceId;Reject([&]{reopened.Submit(another);});
            Require(!Exists(options.auditRoot+"/job-another-copy") && ReadFile(jobRoot+"/child.json",4096)==oldChild,"Unknown job permitted fresh child intent");
            TransferReconciliationRequest verify;verify.operationId=request.operationId;verify.verificationId="manual-check";verify.instanceId=reopened.Identity().instanceId;verify.policySha256=request.policySha256;verify.originalRequestKey=original.requestKey;
            Require(DecodeTransferReconciliationRequest(EncodeTransferReconciliationRequest(verify)).originalRequestKey==original.requestKey,"Verification protocol changed original key");
            auto wrong=verify;wrong.instanceId=request.instanceId;Reject([&]{reopened.Reconcile(wrong);});wrong=verify;wrong.originalRequestKey=std::string(64,'e');Reject([&]{reopened.Reconcile(wrong);});
            Fd destinationLock(::open((original.destination+"/.getdaq-transfer.lock").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC));Require(destinationLock.value>=0 && !::flock(destinationLock.value,LOCK_EX|LOCK_NB),"Hold destination as a live copy");
            auto locked=verify;locked.verificationId="locked-check";reopened.Reconcile(locked);Require(Terminal(reopened,request.operationId).phase=="unknown","Verification bypassed a retained copy lock");
            Require(!::flock(destinationLock.value,LOCK_UN),"Release simulated copy lock");
            Fd copiedFile(::open((original.destination+"/source_0/data0.graw").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC));char priorByte=0;
            Require(copiedFile.value>=0 && ::pread(copiedFile.value,&priorByte,1,17)==1,"Read original copied byte");const char damaged=char(priorByte^1);Require(::pwrite(copiedFile.value,&damaged,1,17)==1,"Damage one copied byte without size change");
            auto corruptCheck=verify;corruptCheck.verificationId="corrupt-check";reopened.Reconcile(corruptCheck);Require(Terminal(reopened,request.operationId).phase=="unknown","Corrupt full copy was promoted complete");
            char retained=0;Require(::pread(copiedFile.value,&retained,1,17)==1 && retained==damaged,"Verification rewrote damaged payload");
            Require(::pwrite(copiedFile.value,&priorByte,1,17)==1,"Restore test-owned copied byte");
            // The source catalog is absent: verification must use the complete
            // independent destination, without recopying or rewriting old PID.
            Require(!::rename(original.source.c_str(),(original.source+"-offline").c_str()),"Move source catalog offline");
            Require(reopened.Reconcile(verify).phase=="verifying","Manual native verification not accepted");
            const auto reconciled=Terminal(reopened,request.operationId);
            Require(reconciled.phase=="complete" && reconciled.request.instanceId==request.instanceId && reconciled.receipt==original.receipt,"Verification changed original identity/receipt or failed");
            Require(ReadFile(jobRoot+"/child.json",4096)==oldChild,"Reconciliation adopted/replayed old copy child");
            const auto verifier=ReadFile(jobRoot+"/verify-manual-check/child.json",4096);
            Require(reopened.Reconcile(verify).phase=="complete" && ReadFile(jobRoot+"/verify-manual-check/child.json",4096)==verifier,"Duplicate verification replayed native child");
            Require(!::rename((original.source+"-offline").c_str(),original.source.c_str()),"Restore source catalog");
            another.operationId="after-reconciliation";reopened.Submit(another);Require(Terminal(reopened,another.operationId).phase=="complete","Verified original did not release new-copy gate");
            std::ofstream mutate(options.auditRoot+"/policy.json",std::ios::app);mutate<<' ';mutate.close();Reject([&]{reopened.SourceReceipt("Test",0);});
            {std::ofstream restore(options.auditRoot+"/policy.json");restore<<"{}";}Reject([&]{reopened.SourceReceipt("Test",0);});
        }
        {
            Fixture changed;{ReceiverCatalog catalog(changed.options);catalog.Execute();}
            CreateDirectory(changed.root+"/backups");CreateDirectory(changed.root+"/jobs");
            auto other=options;other.sourceRoot=changed.options.receiver.catalogRoot;other.backupRoot=changed.root+"/backups";other.auditRoot=changed.root+"/jobs";
            TransferOperation saved;
            {TransferJobs jobs(other);auto req=request;req.instanceId=jobs.Identity().instanceId;req.policySha256=jobs.Identity().policySha256;req.sourceRequestKey=String(ParseStrictJson(jobs.SourceReceipt("Test",0)).At("request_key"));jobs.Submit(req);saved=Terminal(jobs,req.operationId);Require(saved.phase=="complete","Second fixture failed");}
            {std::ofstream mutate(other.auditRoot+"/job-test-copy/request.json",std::ios::app);mutate<<' ';}Reject([&]{TransferJobs reopened(other);});
        }
        std::cout<<"PASS native transfer controller: original source key, separate actual child, progress, ID idempotence, changed-request denial, completed history, policy corruption; source/raw owned cleanup\n";
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
