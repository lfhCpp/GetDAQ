#include "cobolifecyclejournal.h"
#include "configurationsha256.h"
#include "hardwareclient.h"
#include "strictjson.h"
#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
const std::size_t Budget = 16*1024*1024;
const char *Seed = "getdaq.cobo-lifecycle-chain:1\n";
const char *CheckpointSchema = "getdaq.cobo-lifecycle-checkpoint:1";
void Require(bool ok,const char *message) {
    if(!ok)throw std::runtime_error(message);
}
bool Sha256(const std::string &text) {
    return text.size()==64 && text.find_first_not_of("0123456789abcdef")==std::string::npos;
}
void ValidateCheckpoint(const CoBoLifecycleCheckpoint &value) {
    Require(Sha256(value.scopeSha256) && Sha256(value.chainSha256) && value.bytes>0 && value.bytes<=Budget &&
            value.transitions<value.bytes,"Invalid CoBo lifecycle checkpoint");
}
std::uint64_t CheckpointNumber(const JsonValue &value) {
    Require(value.type==JsonValue::Number && !value.text.empty() && value.text.size()<=8 &&
            value.text.find_first_not_of("0123456789")==std::string::npos,
            "CoBo lifecycle checkpoint requires bounded unsigned integers");
    const auto number=std::stoull(value.text);
    Require(number<=Budget && std::to_string(number)==value.text,"Invalid CoBo lifecycle checkpoint integer");
    return number;
}
void ValidatePath(const std::string &path) {
    Require(!path.empty() && path.size()<=4096 && path.back()!='/',"Invalid CoBo lifecycle journal path");
    for(unsigned char c:path)Require(c>=32 && c!=127,"Invalid CoBo lifecycle journal path byte");
}
void Identity(int fd,const std::string &path) {
    struct stat opened{},named{};
    Require(::fstat(fd,&opened)==0 && ::lstat(path.c_str(),&named)==0 &&
            S_ISREG(opened.st_mode) && S_ISREG(named.st_mode) && opened.st_dev==named.st_dev &&
            opened.st_ino==named.st_ino && opened.st_uid==::geteuid() &&
            (opened.st_mode&07777)==0600 && opened.st_nlink==1 && opened.st_size>=0 &&
            static_cast<std::uint64_t>(opened.st_size)<=Budget,
            "CoBo lifecycle journal identity/permissions/budget changed");
}
std::string ReadBytes(int fd) {
    struct stat st{};
    Require(::fstat(fd,&st)==0 && st.st_size>=0 && static_cast<std::uint64_t>(st.st_size)<=Budget,
            "Invalid CoBo lifecycle journal size");
    std::string bytes(static_cast<std::size_t>(st.st_size),'\0');
    std::size_t pos=0;
    while(pos<bytes.size()) {
        const auto n=::pread(fd,&bytes[pos],bytes.size()-pos,static_cast<off_t>(pos));
        if(n<0 && errno==EINTR)continue;
        Require(n>0,"Cannot read CoBo lifecycle journal"); pos+=static_cast<std::size_t>(n);
    }
    return bytes;
}
std::string Header(const std::string &scope,const CoBoLifecycleJournal::Topology &topology) {
    Require(scope.size()==64 && scope.find_first_not_of("0123456789abcdef")==std::string::npos,
            "CoBo lifecycle scope must be SHA256");
    CoBoLifecycleLedger validate(topology);
    std::string row="{\"schema_version\":1,\"scope_sha256\":"+JsonString(scope)+",\"topology\":[";
    for(std::size_t i=0;i<topology.size();++i) {
        Require(topology[i].first.size()<=4096,"CoBo lifecycle ID exceeds journal budget");
        ValidateJsonUtf8(topology[i].first);
        if(i)row+=',';
        row+="{\"id\":"+JsonString(topology[i].first)+",\"enabled\":"+(topology[i].second ? "true" : "false")+"}";
    }
    return row+"]}";
}
std::string TransitionRecord(std::uint64_t sequence,const std::string &id,std::size_t index,
                             CoBoLifecycleState from,CoBoLifecycleState next) {
    return "{\"sequence\":"+std::to_string(sequence)+",\"index\":"+std::to_string(index)+",\"id\":"+JsonString(id)+
           ",\"from\":"+JsonString(CoBoLifecycleStateName(from))+",\"state\":"+JsonString(CoBoLifecycleStateName(next))+"}";
}
std::string Envelope(const std::string &record,const std::string &previous,std::string &chain) {
    chain=ConfigurationSha256(previous+'\n'+record);
    return "{\"record\":"+record+",\"sha256\":"+JsonString(chain)+"}\n";
}
CoBoLifecycleState State(const JsonValue &value) {
    Require(value.type==JsonValue::String,"Invalid CoBo lifecycle state type");
    const CoBoLifecycleState states[]={CoBoLifecycleState::Fresh,CoBoLifecycleState::Preparing,CoBoLifecycleState::Prepared,
        CoBoLifecycleState::Configuring,CoBoLifecycleState::Configured,CoBoLifecycleState::Ready,CoBoLifecycleState::Starting,
        CoBoLifecycleState::Running,CoBoLifecycleState::Paused,CoBoLifecycleState::Resuming,CoBoLifecycleState::Stopping,
        CoBoLifecycleState::Stopped,CoBoLifecycleState::Unknown,CoBoLifecycleState::Disabled};
    for(auto state:states)if(value.text==CoBoLifecycleStateName(state))return state;
    throw std::runtime_error("Unknown CoBo lifecycle journal state");
}
std::size_t Index(const JsonValue &value,std::size_t count) {
    Require(value.type==JsonValue::Number,"Invalid CoBo lifecycle index type");
    for(std::size_t i=0;i<count;++i)if(value.text==std::to_string(i))return i;
    throw std::runtime_error("Invalid CoBo lifecycle fixed index");
}
}

std::string EncodeCoBoLifecycleCheckpoint(const CoBoLifecycleCheckpoint &checkpoint) {
    ValidateCheckpoint(checkpoint);
    return "{\"schema\":"+JsonString(CheckpointSchema)+",\"scope_sha256\":"+JsonString(checkpoint.scopeSha256)+
           ",\"transitions\":"+std::to_string(checkpoint.transitions)+",\"bytes\":"+std::to_string(checkpoint.bytes)+
           ",\"chain_sha256\":"+JsonString(checkpoint.chainSha256)+"}";
}
CoBoLifecycleCheckpoint DecodeCoBoLifecycleCheckpoint(const std::string &json) {
    Require(json.size()<=1024,"CoBo lifecycle checkpoint JSON exceeds budget");
    const auto value=ParseStrictJson(json);
    Require(value.type==JsonValue::Object && value.members.size()==5 && value.At("schema").type==JsonValue::String &&
            value.At("schema").text==CheckpointSchema && value.At("scope_sha256").type==JsonValue::String &&
            value.At("chain_sha256").type==JsonValue::String,"Invalid CoBo lifecycle checkpoint schema");
    CoBoLifecycleCheckpoint checkpoint;
    checkpoint.scopeSha256=value.At("scope_sha256").text;checkpoint.chainSha256=value.At("chain_sha256").text;
    checkpoint.transitions=CheckpointNumber(value.At("transitions"));checkpoint.bytes=CheckpointNumber(value.At("bytes"));
    ValidateCheckpoint(checkpoint);return checkpoint;
}

CoBoLifecycleJournal::CoBoLifecycleJournal(const std::string &path,const std::string &scope,const Topology &topology)
    :fPath(path),fChain(ConfigurationSha256(Seed)),fScope(scope) {
    ValidatePath(path); const auto header=Header(scope,topology);
    fFd=::open(path.c_str(),O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    Require(fFd>=0,"Cannot exclusively create CoBo lifecycle journal");
    try {
        Require(::flock(fFd,LOCK_EX|LOCK_NB)==0,"Cannot lock CoBo lifecycle journal");
        const auto slash=path.find_last_of('/');
        const auto parent=slash==std::string::npos ? "." : slash==0 ? "/" : path.substr(0,slash);
        const int dir=::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
        const bool synced=dir>=0 && ::fsync(dir)==0;
        if(dir>=0)::close(dir);
        Require(synced,"Cannot sync CoBo lifecycle journal directory");
        Persist(header);
    } catch(...) {::close(fFd);fFd=-1;throw;}
}
CoBoLifecycleJournal::~CoBoLifecycleJournal() {if(fFd>=0)::close(fFd);}
void CoBoLifecycleJournal::VerifyFile() const {
    Identity(fFd,fPath);
    Require(ReadBytes(fFd)==fBytes,"CoBo lifecycle journal prefix changed");
}
void CoBoLifecycleJournal::RequireHealthy() const {
    Require(!fFailed,"CoBo lifecycle persistence is unavailable; explicit reconciliation required");
    try {VerifyFile();} catch(...) {fFailed=true;throw;}
}
CoBoLifecycleCheckpoint CoBoLifecycleJournal::Checkpoint() const {
    RequireHealthy();
    CoBoLifecycleCheckpoint checkpoint;
    checkpoint.scopeSha256=fScope;checkpoint.chainSha256=fChain;
    checkpoint.transitions=fSequence;checkpoint.bytes=fBytes.size();return checkpoint;
}
void CoBoLifecycleJournal::Persist(const std::string &record) {
    try {
        RequireHealthy();
        std::string chain; const auto line=Envelope(record,fChain,chain);
        Require(fBytes.size()+line.size()<=Budget,"CoBo lifecycle journal budget exceeded");
        // Allocate before touching the file: a failed append may never be retried.
        auto complete=fBytes+line;
        Require(::lseek(fFd,static_cast<off_t>(fBytes.size()),SEEK_SET)>=0,"Cannot seek CoBo lifecycle journal");
        std::size_t sent=0;
        while(sent<line.size()) {
            const auto n=::write(fFd,line.data()+sent,line.size()-sent);
            if(n<0 && errno==EINTR)continue;
            Require(n>0,"Cannot write CoBo lifecycle journal"); sent+=static_cast<std::size_t>(n);
        }
        Require(::fsync(fFd)==0,"Cannot sync CoBo lifecycle journal");
        fBytes.swap(complete); fChain.swap(chain);
        VerifyFile();
    } catch(...) {fFailed=true;throw;}
}
void CoBoLifecycleJournal::Append(const std::string &id,std::size_t index,CoBoLifecycleState from,CoBoLifecycleState next) {
    Persist(TransitionRecord(fSequence+1,id,index,from,next)); ++fSequence;
}

CoBoLifecycleHistory CoBoLifecycleJournal::ReadHistory(const std::string &path,const std::string &scope,const Topology &topology) {
    ValidatePath(path); const auto header=Header(scope,topology);
    const int fd=::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
    Require(fd>=0,"Cannot open CoBo lifecycle history");
    std::string bytes;
    try {
        Require(::flock(fd,LOCK_SH|LOCK_NB)==0,"CoBo lifecycle journal still has an active writer");
        Identity(fd,path); bytes=ReadBytes(fd); Identity(fd,path);
        Require(ReadBytes(fd)==bytes,"CoBo lifecycle history changed while reading");
        ::close(fd);
    } catch(...) {::close(fd);throw;}
    Require(!bytes.empty() && bytes.back()=='\n',"Incomplete CoBo lifecycle history record");
    CoBoLifecycleLedger replay(topology);
    CoBoLifecycleHistory result; result.chainSha256=ConfigurationSha256(Seed);
    std::size_t pos=0;
    while(pos<bytes.size()) {
        const auto end=bytes.find('\n',pos);
        const auto line=bytes.substr(pos,end-pos+1);
        std::string record=header;
        if(pos) {
            const auto envelope=ParseStrictJson(line); const auto &row=envelope.At("record");
            const auto i=Index(row.At("index"),topology.size());
            const auto from=State(row.At("from")),next=State(row.At("state"));
            Require(replay.StateAt(i)==from,"CoBo lifecycle history predecessor mismatch");
            record=TransitionRecord(result.transitions+1,topology[i].first,i,from,next);
            replay.Transition(topology[i].first,i,next); ++result.transitions;
        }
        std::string chain;
        Require(line==Envelope(record,result.chainSha256,chain),"CoBo lifecycle history scope/topology/sequence/hash mismatch");
        result.chainSha256=chain; pos=end+1;
    }
    result.states=replay.Snapshot();result.bytes=bytes.size();return result;
}
CoBoLifecycleHistory CoBoLifecycleJournal::ReadHistory(const std::string &path,const std::string &scope,const Topology &topology,
                                                     const CoBoLifecycleCheckpoint &expected) {
    ValidateCheckpoint(expected);
    Require(expected.scopeSha256==scope,"CoBo lifecycle checkpoint scope mismatch");
    auto history=ReadHistory(path,scope,topology);
    Require(history.transitions==expected.transitions && history.bytes==expected.bytes && history.chainSha256==expected.chainSha256,
            "CoBo lifecycle history terminal checkpoint mismatch");
    history.terminalVerified=true;return history;
}
}
