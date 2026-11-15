#include "nativeeccfilesystemproof_p.h"
#include "hardwareaudit.h"

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {
using namespace GetDAQ;
using namespace NativeEccFilesystemProof;

void Require(bool value,const char *message) {
    if (!value) throw std::runtime_error(message);
}
void Reject(const std::function<void()> &action,const char *message) {
    bool rejected=false;
    try { action(); } catch (const std::exception &) { rejected=true; }
    Require(rejected,message);
}

struct Fixture {
    std::string root;
    std::vector<std::string> paths;
    Fixture() {
        char path[]="/tmp/getdaq-native-proof-XXXXXX";
        if (!::mkdtemp(path)) throw std::runtime_error("Create filesystem proof fixture");
        root=path;
    }
    ~Fixture() {
        for (const auto &path:paths) ::unlink(path.c_str());
        ::rmdir(root.c_str());
    }
    std::string Path(const std::string &name) {
        const auto path=root+'/'+name; paths.push_back(path); return path;
    }
};

void Run() {
    Fixture fixture;
    const auto immutable=fixture.Path("immutable.json");
    const auto immutableLink=fixture.Path("immutable-link.json");
    const auto auditPath=fixture.Path("failed-describe.jsonl");
    const auto auditLink=fixture.Path("audit-link.jsonl");
    const auto directoryLink=fixture.root+"-link";
    fixture.paths.push_back(directoryLink);
    {
        Directory directory(fixture.root); directory.Check();
        Require(::chmod(fixture.root.c_str(),0755)==0,"Change directory mode fixture");
        Reject([&] { directory.Check(); },"Directory proof accepted changed permissions");
        Require(::chmod(fixture.root.c_str(),0700)==0,"Restore directory mode fixture"); directory.Check();
        WriteNewFile(immutable,"intent\n");
        struct stat st{};
        Require(::lstat(immutable.c_str(),&st)==0 && S_ISREG(st.st_mode) &&
            st.st_uid==::geteuid() && (st.st_mode&0777)==0600 && st.st_nlink==1,
            "New proof file identity/owner/mode/link count differs");
        Reject([&] { WriteNewFile(immutable,"replacement\n"); },"Exclusive proof file creation overwrote an existing file");
        ImmutableFile proof(immutable,"intent\n"); proof.Check();
        const int fd=::open(immutable.c_str(),O_WRONLY|O_APPEND|O_CLOEXEC);
        Require(fd>=0,"Open proof file for authorized append fixture");
        const char suffix[]="complete\n";
        Require(::write(fd,suffix,sizeof(suffix)-1)==ssize_t(sizeof(suffix)-1) && ::fsync(fd)==0,
            "Append proof suffix");
        ::close(fd);
        proof.AdvanceVerifiedAppend("intent\ncomplete\n"); proof.Check();
        Reject([&] { proof.AdvanceVerifiedAppend("different\n"); },"Proof accepted a changed retained prefix");
        const int tamper=::open(immutable.c_str(),O_WRONLY|O_TRUNC|O_CLOEXEC);
        Require(tamper>=0,"Open proof content tamper fixture");
        const std::string forged(16,'x');
        Require(::write(tamper,forged.data(),forged.size())==ssize_t(forged.size()) && ::fsync(tamper)==0,
            "Write proof content tamper fixture"); ::close(tamper);
        Reject([&] { proof.Check(); },"Immutable proof accepted same-size content tampering");
        Require(::symlink(immutable.c_str(),immutableLink.c_str())==0,"Create immutable-file symlink fixture");
        Reject([&] { ImmutableFile linked(immutableLink,forged); },"Immutable proof followed a symlink");
        Require(::symlink(fixture.root.c_str(),directoryLink.c_str())==0,"Create directory symlink fixture");
        Reject([&] { Directory linked(directoryLink); },"Directory proof followed a symlink");
    }
    std::string chain;
    {
        HardwareAudit audit(auditPath); audit.Append("{\"phase\":\"failed\"}"); chain=audit.ChainSha256();
    }
    struct stat identity{};
    Require(::lstat(auditPath.c_str(),&identity)==0,"Inspect audit file identity");
    const auto bytes=ReadFailedDescriptionAudit(auditPath,identity,chain);
    Require(bytes=="{\"phase\":\"failed\"}\n","Captured failed audit bytes differ");
    Reject([&] { ReadFailedDescriptionAudit(auditPath,identity,std::string(64,'0')); },
        "Failed-description audit accepted an incorrect chain digest");
    Require(::symlink(auditPath.c_str(),auditLink.c_str())==0,"Create audit symlink fixture");
    Reject([&] { ReadFailedDescriptionAudit(auditLink,identity,chain); },"Failed-description audit capture followed a symlink");
    std::cout<<"PASS Native ECC directory/file identity, create-only write, verified append and audit-chain capture\n";
}
}

int main() {
    try { Run(); return 0; }
    catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
