#include "datasupervisor_p.h"
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <map>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ {
using namespace DataSupervisorInternal;

void DataSupervisor::Impl::Identity(const JsonValue &s,const Child &c,const ReceiverSourcePlan &source,bool seal) const {
        if (Unsigned(s.At("control_protocol"))!=1 || Unsigned(s.At("pid"))!=std::uint64_t(c.status.pid) || String(s.At("session_id"))!=session ||
            String(s.At("sender_id"))!=source.id || String(s.At("configuration_sha256"))!=options.configurationSha256 || String(s.At("artifact_sha256"))!=options.artifactSha256 ||
            String(s.At("output_directory"))!=c.status.outputDirectory) throw std::runtime_error("Receiver PID/session/source/configuration identity differs");
        if (!Boolean(s.At("durable"))) throw std::runtime_error("Receiver did not durably sync data");
        if (!seal) {
            const auto naming=s.members.find("file_naming");
            const auto actual=naming==s.members.end() ? "native" : String(naming->second);
            if (actual!=source.fileNaming) throw std::runtime_error("Receiver file naming differs from the fixed plan");
            const auto protocol=s.members.find("input_protocol");
            const auto actualProtocol=protocol==s.members.end() ? "TCP" : String(protocol->second);
            if (actualProtocol!=source.inputProtocol) throw std::runtime_error("Receiver input protocol differs from the fixed plan");
            const auto storage=s.members.find("storage_mode");
            const auto actualStorage=storage==s.members.end() ? "frame-storage" : String(storage->second);
            if (actualStorage!=source.storageMode) throw std::runtime_error("Receiver storage mode differs from the fixed plan");
        } else {
            const auto storage=s.members.find("storage_mode");
            const auto actualStorage=storage==s.members.end() ? "frame-storage" : String(storage->second);
            if (actualStorage!=source.storageMode) throw std::runtime_error("Receiver seal storage mode differs from the fixed plan");
        }
        const bool cycle=source.inputProtocol=="FDT";
        if (!seal && (String(s.At("service"))!=(source.inputProtocol=="ICE" ? "getdaq-ice-receiver" : "getdaq-receiver") || Boolean(s.At("once"))==cycle || Unsigned(s.At("data_port"))!=source.dataPort ||
                     Unsigned(s.At("status_port"))!=source.statusPort || String(s.At("data_bind"))!=source.bindAddress)) throw std::runtime_error("Receiver runtime options differ");
        if (!seal && cycle && (String(s.At("cycle_control"))!="stdin-socket-v1" || !Boolean(s.At("cycle_control_connected"))))
            throw std::runtime_error("Receiver cycle control is unavailable or differs");
    }

void DataSupervisor::Impl::LockHeld(Child &c,bool bind) const {
        c.directory->Check(); const auto path=c.status.outputDirectory+"/.getdaq-receiver.lock";
        Fd fd(::open(path.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC)); struct stat st;
        if (fd.value<0 || ::fstat(fd.value,&st) || !S_ISREG(st.st_mode)) throw std::runtime_error("Receiver directory lock missing");
        if (c.lockBound && !Same(st,c.lockIdentity)) throw std::runtime_error("Receiver directory lock identity changed");
        if (::flock(fd.value,LOCK_EX|LOCK_NB)==0) { ::flock(fd.value,LOCK_UN); throw std::runtime_error("Receiver does not hold its output directory lock"); }
        if (errno!=EWOULDBLOCK && errno!=EAGAIN) throw std::runtime_error("Cannot verify receiver output lock");
        if (!c.lockBound) {
            if (!bind) throw std::runtime_error("Receiver output lock has not been bound at startup");
            c.lockIdentity=st; c.lockBound=true;
        }
    }

JsonValue DataSupervisor::Impl::ReadyStatus(std::size_t i,bool bind) const {
        auto &c=*children[i]; const auto &source=options.sources[i];
        if (c.status.reaped || Exited(c.status.pid)) throw std::runtime_error("Managed receiver exited before data lifecycle completed");
        const auto status=ParseStrictJson(HttpStatus(source.statusPort,options.statusTimeoutMs)); Identity(status,c,source); LockHeld(c,bind);
        const bool cycle=source.inputProtocol=="FDT";
        if (!String(status.At("error")).empty() || (cycle ? Boolean(status.At("cycle_finish_requested")) :
            Boolean(status.At("sender_eof_confirmed")) || Unsigned(status.At("connections"))>1))
            throw std::runtime_error("Managed receiver is failed or already finishing");
        if (source.inputProtocol=="ICE" && (Boolean(status.At("sender_end_confirmed")) || Boolean(status.At("sender_eof_confirmed"))))
            throw std::runtime_error("ICE receiver is already finishing or falsely reports TCP EOF");
        return status;
    }

const std::string &DataSupervisor::Impl::Executable(std::size_t i) const {return options.sources[i].inputProtocol=="ICE" ? options.iceReceiverExecutable : options.receiverExecutable;}

std::vector<std::string> DataSupervisor::Impl::Arguments(std::size_t i,const Child &c) const {
        const auto &s=options.sources[i];
        std::vector<std::string> arguments={Executable(i),"--output",c.status.outputDirectory,"--bind",s.bindAddress,"--port",std::to_string(s.dataPort),
            "--status-bind","127.0.0.1","--status-port",std::to_string(s.statusPort),"--export-port",std::to_string(s.exportPort),
            "--queue-mib",std::to_string(s.queueMiB),"--max-frame-mib",std::to_string(s.maxFrameMiB),"--rotate-mib",std::to_string(s.rotateMiB),
            "--export-queue-mib",std::to_string(s.exportQueueMiB),"--session-id",session,"--sender-id",s.id,
            "--configuration-sha256",options.configurationSha256,"--artifact-sha256",options.artifactSha256};
        if (s.minFreeMiB) {arguments.push_back("--min-free-mib");arguments.push_back(std::to_string(s.minFreeMiB));}
        if (s.fileNaming!="native") {arguments.push_back("--file-naming");arguments.push_back(s.fileNaming);}
        if (s.inputProtocol!="TCP") {arguments.push_back("--input-protocol");arguments.push_back(s.inputProtocol);}
        if (s.storageMode!="frame-storage") {arguments.push_back("--storage-mode");arguments.push_back(s.storageMode);}
        arguments.push_back(s.inputProtocol=="FDT" ? "--cycle-control-stdin" : "--once");
        return arguments;
    }

void DataSupervisor::Impl::Seal(std::size_t i,bool unused) {
        auto &c=*children[i]; const auto &source=options.sources[i]; c.directory->Check();
        Fd leaseFd(::open((c.status.outputDirectory+"/.getdaq-receiver.lock").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC)); struct stat st;
        if (leaseFd.value<0 || ::fstat(leaseFd.value,&st) || !c.lockBound || !Same(st,c.lockIdentity) || ::flock(leaseFd.value,LOCK_EX|LOCK_NB))
            throw std::runtime_error("Receiver output lock not released/identity changed after exit");
        const auto out=ReadFile(c.log+".stdout",65536); const auto last=out.find_last_not_of("\r\n");
        if (last==std::string::npos) throw std::runtime_error("Receiver final status missing");
        const auto start=out.rfind('\n',last); const auto status=ParseStrictJson(out.substr(start==std::string::npos ? 0 : start+1,last-(start==std::string::npos ? 0 : start+1)+1));
        Identity(status,c,source);
        const auto connections=Unsigned(status.At("connections")); const bool cycle=source.inputProtocol=="FDT",ice=source.inputProtocol=="ICE";
        const bool confirmedUnused=status.members.count("source_unused_confirmed") && Boolean(status.At("source_unused_confirmed"));
        const bool boundary=unused ? cycle && !connections && !Boolean(status.At("sender_eof_confirmed")) && confirmedUnused && !Boolean(status.At("stream_sealed")) :
            !confirmedUnused && (ice ? !Boolean(status.At("sender_eof_confirmed")) && Boolean(status.At("sender_end_confirmed")) : Boolean(status.At("sender_eof_confirmed"))) && connections && (cycle || connections==1) && Boolean(status.At("stream_sealed"));
        if (Boolean(status.At("connected")) || !boundary || Unsigned(status.At("queued_blocks")) || !String(status.At("error")).empty())
            throw std::runtime_error("Receiver final status does not prove EOF/drain/seal or requested unused boundary");
        if (cycle && !Boolean(status.At("cycle_finish_requested"))) throw std::runtime_error("Receiver cycle lacks actual finish request");
        const auto receipt=ParseStrictJson(ReadFile(c.status.outputDirectory+(ice ? "/.getdaq-ice-receiver-seal.json" : "/.getdaq-receiver-seal.json"),16*1024*1024)); Identity(receipt,c,source,true);
        const bool receiptUnused=receipt.members.count("source_unused_confirmed") && Boolean(receipt.At("source_unused_confirmed"));
        if (Unsigned(receipt.At("schema"))!=1 || !Boolean(receipt.At("complete")) || (ice ? Boolean(receipt.At("sender_eof_confirmed")) || !Boolean(receipt.At("sender_end_confirmed")) || String(receipt.At("kind"))!="getdaq.ice-receiver-seal" || String(receipt.At("input_protocol"))!="ICE" || String(receipt.At("completion_basis"))!="ordered-same-connection-finishStream" : Boolean(receipt.At("sender_eof_confirmed"))==unused) || receiptUnused!=unused || Unsigned(receipt.At("connections"))!=connections || receipt.At("files").type!=JsonValue::Array)
            throw std::runtime_error("Receiver seal does not prove the expected EOF/session");
        std::map<std::string,std::uint64_t> files; std::uint64_t total=0;
        for (const auto &file:receipt.At("files").items) {
            const auto name=String(file.At("name")); const auto bytes=Unsigned(file.At("bytes"),true);
            if (name.empty() || name.size()>255 || name=="." || name==".." || name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.-:")!=std::string::npos ||
                name.size()<5 || name.substr(name.size()-5)!=".graw" || !files.emplace(name,bytes).second || total>UINT64_MAX-bytes) throw std::runtime_error("Malformed/duplicate receiver seal entry");
            if (::fstatat(c.directory->Descriptor(),name.c_str(),&st,AT_SYMLINK_NOFOLLOW) || !S_ISREG(st.st_mode) || st.st_size<0 || std::uint64_t(st.st_size)!=bytes)
                throw std::runtime_error("Receiver sealed file name/size/type changed");
            total+=bytes;
        }
        const int scan=::openat(c.directory->Descriptor(),".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
        DIR *dir=scan<0 ? nullptr : ::fdopendir(scan);
        if (!dir) { if (scan>=0) ::close(scan); throw std::runtime_error("Cannot enumerate sealed receiver directory"); }
        bool mismatch=false; std::size_t count=0;
        errno=0;
        while (auto *entry=::readdir(dir)) {
            const std::string name=entry->d_name;
            if (name.size()>=5 && name.substr(name.size()-5)==".graw") { ++count; if (!files.count(name)) mismatch=true; }
        }
        const int scanError=errno; ::closedir(dir); c.directory->Check();
        if (scanError) throw std::runtime_error("Cannot finish sealed file enumeration");
        if (mismatch || count!=files.size() || total!=Unsigned(receipt.At("received_bytes"),true) || total!=Unsigned(status.At("received_bytes")) ||
            total!=Unsigned(status.At("accepted_file_bytes")) || total!=Unsigned(status.At("sealed_bytes")) || Unsigned(status.At("files"))!=files.size())
            throw std::runtime_error("Receiver final counters/seal/file set differ");
        if (unused && (total || !files.empty() || Unsigned(status.At("frames")))) throw std::runtime_error("Unused source has data/file counters");
        c.status.receivedBytes=total; c.status.frames=Unsigned(status.At("frames")); c.status.files=files.size(); c.status.sealVerified=true; c.status.sourceUnusedConfirmed=unused; c.status.sourceEndConfirmed=ice;
    }
}
