#include "cobolifecyclejournal.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
using namespace GetDAQ;
using State=CoBoLifecycleState;
void Check(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void Rejects(const std::function<void()> &action,const char *message) {
    bool rejected=false;try {action();} catch(const std::exception &) {rejected=true;}Check(rejected,message);
}
std::string Read(const std::string &path) {
    std::ifstream in(path,std::ios::binary);Check(bool(in),"read fixture");
    return std::string(std::istreambuf_iterator<char>(in),{});
}
void Write(const std::string &path,const std::string &bytes) {
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out<<bytes;out.close();Check(bool(out),"write fixture");
}
struct Fixture {
    std::string directory;
    std::vector<std::string> paths;
    Fixture() {char path[]="/tmp/getdaq-cobo-journal-XXXXXX";Check(::mkdtemp(path)!=nullptr,"temp directory");directory=path;}
    ~Fixture() {for(const auto &path:paths)::unlink(path.c_str());::rmdir(directory.c_str());}
    std::string Path() {const auto p=directory+'/'+std::to_string(paths.size())+".jsonl";paths.push_back(p);return p;}
};
void Configured(CoBoLifecycleLedger &ledger) {
    ledger.Transition("C0",0,State::Preparing);ledger.Transition("C0",0,State::Prepared);
    ledger.Transition("C0",0,State::Configuring);ledger.Transition("C0",0,State::Configured);
}
}
int main() {
    try {
        Fixture fixture;
        const CoBoLifecycleJournal::Topology topology={{"C0",true},{"C1",false}};
        const auto scope=ConfigurationSha256("fixed-preparation-intent");
        const auto path=fixture.Path();
        CoBoLifecycleCheckpoint expected,earlier;
        {CoBoLifecycleLedger ledger(topology);Rejects([&]{ledger.PersistenceCheckpoint();},"volatile ledger fabricated a checkpoint");}
        {
            CoBoLifecycleLedger ledger(topology);ledger.BeginPersistence(path,scope);
            struct stat st{};Check(::stat(path.c_str(),&st)==0 && (st.st_mode&0777)==0600,"journal mode");
            Rejects([&]{ledger.BeginPersistence(fixture.Path(),scope);},"second journal accepted");
            Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology);},"live writer read as cold history");
            const auto header=Read(path);
            const auto initialCheckpoint=ledger.PersistenceCheckpoint();
            Check(initialCheckpoint.transitions==0 && initialCheckpoint.bytes==header.size() && initialCheckpoint.scopeSha256==scope,
                  "initial checkpoint does not bind the persisted header");
            Rejects([&]{ledger.Transition("C1",0,State::Preparing);},"wrong ID accepted");
            Rejects([&]{ledger.Transition("C1",1,State::Preparing);},"disabled board activated");
            Rejects([&]{ledger.TransitionForNegativeStop("C0",0,State::Running);},"negative API granted positive control");
            Check(Read(path)==header,"invalid action changed journal");
            Configured(ledger);ledger.RequireConfiguredForAcquisition();
            earlier=ledger.PersistenceCheckpoint();
            ledger.Transition("C0",0,State::Ready);ledger.Transition("C0",0,State::Starting);
            ledger.Transition("C0",0,State::Running);
            Check(ledger.TransitionForNegativeStop("C0",0,State::Stopping),"stop intent not persisted");
            Check(ledger.TransitionForNegativeStop("C0",0,State::Stopped),"stop result not persisted");
            expected=ledger.PersistenceCheckpoint();
        }
        const auto good=Read(path);
        const auto history=CoBoLifecycleJournal::ReadHistory(path,scope,topology);
        Check(history.transitions==9 && history.states==std::vector<State>({State::Stopped,State::Disabled}) &&
              history.chainSha256.size()==64 && !history.terminalVerified && history.bytes==good.size(),"durable state replay differs");
        const auto encoded=EncodeCoBoLifecycleCheckpoint(expected);
        const auto decoded=DecodeCoBoLifecycleCheckpoint(encoded);
        Check(EncodeCoBoLifecycleCheckpoint(decoded)==encoded,"checkpoint codec changed endpoint");
        const auto anchored=CoBoLifecycleJournal::ReadHistory(path,scope,topology,decoded);
        Check(anchored.terminalVerified && anchored.chainSha256==expected.chainSha256 && anchored.transitions==expected.transitions &&
              anchored.bytes==expected.bytes,"checkpoint did not verify full history");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology,earlier);},"stale checkpoint accepted newer history");
        for(int field=0;field<4;++field) {
            auto wrong=expected;
            if(field==0)wrong.scopeSha256=ConfigurationSha256("wrong checkpoint scope");
            if(field==1)--wrong.bytes;
            if(field==2)--wrong.transitions;
            if(field==3)wrong.chainSha256=ConfigurationSha256("wrong terminal chain");
            Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology,wrong);},"wrong checkpoint field accepted");
        }
        for(const auto &invalid:{std::string("{}"),std::string("null"),encoded.substr(0,encoded.size()-1)+",\"extra\":true}",
                               std::string(1025,' '),std::string("{\"schema\":\"other\"}")})
            Rejects([&]{DecodeCoBoLifecycleCheckpoint(invalid);},"invalid checkpoint JSON accepted");
        for(const auto &number:{"-1","1.0","1e1","\"9\"","16777217","9999999999999999999999999999"}) {
            auto invalid=encoded;const auto start=invalid.find("\"transitions\":9");
            Check(start!=std::string::npos,"missing checkpoint transitions fixture");
            invalid.replace(start,std::string("\"transitions\":9").size(),std::string("\"transitions\":")+number);
            Rejects([&]{DecodeCoBoLifecycleCheckpoint(invalid);},"noncanonical or out-of-budget checkpoint integer accepted");
        }
        // Every whole-record prefix still has valid hashes and transitions, but
        // it must fail when compared with an independently retained endpoint.
        auto prefix=good;
        for(unsigned removed=1;removed<=9;++removed) {
            prefix.resize(prefix.rfind('\n',prefix.size()-2)+1);Write(path,prefix);
            const auto shorter=CoBoLifecycleJournal::ReadHistory(path,scope,topology);
            Check(shorter.transitions==9-removed && !shorter.terminalVerified,"valid historical prefix no longer readable");
            Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology,expected);},"whole-record tail loss escaped endpoint check");
        }
        // A legal alternative terminal state of exactly the same byte length
        // proves the endpoint hash is checked independently of size/count.
        const auto lastStart=good.rfind('\n',good.size()-2)+1;
        const auto previousStart=good.rfind('\n',lastStart-2)+1;
        const auto previous=ParseStrictJson(good.substr(previousStart,lastStart-previousStart)).At("sha256").text;
        const std::string alternative="{\"sequence\":9,\"index\":0,\"id\":\"C0\",\"from\":\"stopping\",\"state\":\"unknown\"}";
        const auto changed=good.substr(0,lastStart)+"{\"record\":"+alternative+",\"sha256\":\""+ConfigurationSha256(previous+'\n'+alternative)+"\"}\n";
        Check(changed.size()==good.size(),"alternative terminal fixture size differs");Write(path,changed);
        Check(CoBoLifecycleJournal::ReadHistory(path,scope,topology).states[0]==State::Unknown,"alternative legal history rejected");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology,expected);},"same-size/count alternative terminal history accepted");
        Write(path,good);
        Rejects([&]{CoBoLifecycleJournal writer(path,scope,topology);},"existing history reopened for writing");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,ConfigurationSha256("other"),topology);},"wrong scope accepted");
        auto other=topology;other[1].second=true;
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,other);},"changed topology accepted");
        other=topology;std::swap(other[0],other[1]);
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,other);},"reordered topology accepted");
        for(const auto &bad:{good.substr(0,good.size()-1),good+"\n",good+"{",std::string("{}\n")}) {
            Write(path,bad);Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology);},"malformed tail/header accepted");
        }
        auto bad=good;bad[bad.find("stopped")]='X';Write(path,bad);
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology);},"bad state/hash accepted");
        // Recompute valid hashes around invalid records: validation must enforce
        // sequence, fixed ID/index, predecessor, transition matrix and schema.
        const auto headerEnd=good.find('\n')+1;
        const auto header=good.substr(0,headerEnd);
        Write(path,header);
        const auto initial=CoBoLifecycleJournal::ReadHistory(path,scope,topology).chainSha256;
        const std::string records[]={
            "{\"sequence\":2,\"index\":0,\"id\":\"C0\",\"from\":\"fresh\",\"state\":\"preparing\"}",
            "{\"sequence\":1,\"index\":0,\"id\":\"C1\",\"from\":\"fresh\",\"state\":\"preparing\"}",
            "{\"sequence\":1,\"index\":2,\"id\":\"C0\",\"from\":\"fresh\",\"state\":\"preparing\"}",
            "{\"sequence\":1,\"index\":0,\"id\":\"C0\",\"from\":\"prepared\",\"state\":\"preparing\"}",
            "{\"sequence\":1,\"index\":0,\"id\":\"C0\",\"from\":\"fresh\",\"state\":\"running\"}",
            "{\"sequence\":1,\"index\":1,\"id\":\"C1\",\"from\":\"disabled\",\"state\":\"preparing\"}",
            "{\"sequence\":1,\"index\":0,\"id\":\"C0\",\"from\":\"fresh\",\"state\":\"preparing\",\"extra\":true}"};
        for(const auto &record:records) {
            Write(path,header+"{\"record\":"+record+",\"sha256\":\""+ConfigurationSha256(initial+'\n'+record)+"\"}\n");
            Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology);},"hash-valid invalid transition accepted");
        }
        Write(path,good);
        const auto alias=fixture.Path();Check(::symlink(path.c_str(),alias.c_str())==0,"make symlink");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(alias,scope,topology);},"symlink history accepted");
        Check(::chmod(path.c_str(),0644)==0,"change mode");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(path,scope,topology);},"public history accepted");
        Check(::chmod(path.c_str(),0600)==0,"restore mode");

        for(int fault=0;fault<5;++fault) {
            const auto file=fixture.Path();CoBoLifecycleLedger ledger(topology);ledger.BeginPersistence(file,scope);Configured(ledger);
            const auto saved=Read(file);
            if(fault==0)Write(file,saved.substr(0,saved.size()-1));
            if(fault==1) {auto altered=saved;altered[5]='X';Write(file,altered);}
            if(fault==2)Write(file,saved+"unexpected\n");
            if(fault==3)Check(::chmod(file.c_str(),0644)==0,"writer chmod");
            if(fault==4) {const auto moved=fixture.Path();Check(::rename(file.c_str(),moved.c_str())==0,"rename writer");Write(file,saved);Check(::chmod(file.c_str(),0600)==0,"replacement mode");}
            Rejects([&]{ledger.Transition("C0",0,State::Ready);},"damaged writer published Ready");
            Rejects([&]{ledger.PersistenceCheckpoint();},"damaged writer exported trusted checkpoint");
            Check(ledger.StateAt(0)==State::Unknown && ledger.StateAt(1)==State::Disabled,"failure failed to isolate UNKNOWN");
            // Restoring bytes/mode cannot clear a latched append failure.
            Write(file,saved);Check(::chmod(file.c_str(),0600)==0,"restore writer");
            Rejects([&]{ledger.Transition("C0",0,State::Prepared);},"failed writer silently resumed");
            Rejects([&]{ledger.RequireConfiguredForAcquisition();},"failed journal granted acquisition");
            Check(!ledger.TransitionForNegativeStop("C0",0,State::Stopping) &&
                  !ledger.TransitionForNegativeStop("C0",0,State::Stopped) && ledger.StateAt(0)==State::Unknown,
                  "negative stop claimed a durable state after journal failure");
            Rejects([&]{ledger.MarkUnknown("C0",0);},"failed UNKNOWN journal unexpectedly succeeded");
            Check(ledger.StateAt(0)==State::Unknown,"UNKNOWN lost on write failure");
        }
        {CoBoLifecycleLedger ledger(topology);ledger.Transition("C0",0,State::Preparing);
         Rejects([&]{ledger.BeginPersistence(fixture.Path(),scope);},"late journal attachment accepted");}

        // A real partial write, not just pre-append corruption. Keep the resource
        // limit and signal disposition confined to the child process.
        const auto partial=fixture.Path();
        const auto child=::fork();Check(child>=0,"fork partial-write child");
        if(!child) {
            try {
                CoBoLifecycleLedger ledger(topology);ledger.BeginPersistence(partial,scope);
                struct rlimit saved{};Check(::getrlimit(RLIMIT_FSIZE,&saved)==0,"read size limit");
                auto limit=saved;limit.rlim_cur=Read(partial).size()+20;
                std::signal(SIGXFSZ,SIG_IGN);Check(::setrlimit(RLIMIT_FSIZE,&limit)==0,"set size limit");
                Rejects([&]{ledger.Transition("C0",0,State::Preparing);},"partial write accepted");
                Check(ledger.StateAt(0)==State::Unknown,"partial write retained old state");
                Check(::setrlimit(RLIMIT_FSIZE,&saved)==0,"restore size limit");
                Rejects([&]{ledger.Transition("C0",0,State::Prepared);},"partial append retried");
                ::_exit(0);
            } catch(...) {::_exit(1);}
        }
        int status=0;Check(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"partial-write child failed");
        Rejects([&]{CoBoLifecycleJournal::ReadHistory(partial,scope,topology);},"partial-write history accepted");
        const auto interrupted=fixture.Path();
        const auto crashed=::fork();Check(crashed>=0,"fork interrupted child");
        if(!crashed) {try {CoBoLifecycleLedger ledger(topology);ledger.BeginPersistence(interrupted,scope);
            ledger.Transition("C0",0,State::Preparing);::_exit(0);} catch(...) {::_exit(1);}}
        Check(::waitpid(crashed,&status,0)==crashed && WIFEXITED(status) && WEXITSTATUS(status)==0,"interrupted child failed");
        Check(CoBoLifecycleJournal::ReadHistory(interrupted,scope,topology).states[0]==State::Preparing,
              "restart erased unfinished Preparing evidence");
        std::cout<<"PASS durable CoBo lifecycle history, validation, writer faults, partial write and interrupted process\n";
        return EXIT_SUCCESS;
    } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return EXIT_FAILURE;}
}
