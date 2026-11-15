#include "hostcontrolownership.h"
#include "configurationsha256.h"
#include <dirent.h>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <csignal>
#include <unistd.h>
using namespace GetDAQ;
namespace {
void Require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
void Rejected(const std::function<void()> &action) {
    bool rejected=false; try { action(); } catch (const std::exception &) { rejected=true; }
    Require(rejected,"Expected ownership rejection");
}
struct Temporary {
    std::string path;
    Temporary() { char name[]="/tmp/getdaq-host-ownership-XXXXXX"; const auto p=::mkdtemp(name); Require(p,"mkdtemp failed"); path=p; }
    ~Temporary() {
        DIR *directory=::opendir(path.c_str());
        if (directory) { while (const auto entry=::readdir(directory)) {
            const std::string name=entry->d_name;
            if (name!="." && name!="..") ::unlink((path+'/'+name).c_str());
        } ::closedir(directory); }
        ::rmdir(path.c_str());
    }
};
std::string File(const std::string &directory,const std::string &resource) {
    return directory+'/'+ConfigurationSha256("getdaq.host-board:1\n"+resource)+".lock";
}
}
int main() {
    try {
        Temporary temporary; const auto root=temporary.path; const std::string digest(64,'a');
        {
            HostControlOwnership owner(root,{"127.0.0.2","127.0.0.1","127.0.0.2"},"first",digest);
            Require(owner.Resources().size()==2,"Resources were not deduplicated"); owner.Check();
            Rejected([&] { HostControlOwnership second(root,{"127.0.0.1"},"another-deployment",std::string(64,'b')); });
            HostControlOwnership disjoint(root,{"127.0.0.3"},"disjoint",digest); disjoint.Check();
            Rejected([&] { HostControlOwnership partial(root,{"127.0.0.10","127.0.0.2"},"partial",digest); });
            HostControlOwnership rollback(root,{"127.0.0.10"},"rollback",digest); rollback.Check();
            const pid_t child=::fork(); Require(child>=0,"fork failed");
            if (!child) {
                try { HostControlOwnership contender(root,{"127.0.0.2"},"child",digest); ::_exit(1); }
                catch (...) { ::_exit(0); }
            }
            int status=0; Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"Separate process was not excluded");
        }
        HostControlOwnership reacquired(root,{"127.0.0.1"},"new",digest); reacquired.Check();
        int ready[2],finish[2]; Require(::pipe(ready)==0 && ::pipe(finish)==0,"pipe failed");
        const pid_t holder=::fork(); Require(holder>=0,"fork holder failed");
        if (!holder) {
            ::close(ready[0]); ::close(finish[1]);
            try {
                HostControlOwnership child(root,{"127.0.0.4"},"temporary-holder",digest);
                const char byte='x'; if (::write(ready[1],&byte,1)!=1) ::_exit(2);
                char response=0; const auto received=::read(finish[0],&response,1); ::_exit(received<0 ? 4 : 0);
            } catch (...) { ::_exit(3); }
        }
        ::close(ready[1]); ::close(finish[0]); char response=0;
        Require(::read(ready[0],&response,1)==1,"Child holder did not acquire"); ::close(ready[0]);
        Rejected([&] { HostControlOwnership contender(root,{"127.0.0.4"},"contender",digest); });
        Require(::kill(holder,SIGKILL)==0,"Cannot stop own holder fixture");
        int holderStatus=0; Require(::waitpid(holder,&holderStatus,0)==holder && WIFSIGNALED(holderStatus),"Holder did not terminate");
        ::close(finish[1]);
        HostControlOwnership afterCrash(root,{"127.0.0.4"},"after-crash",digest); afterCrash.Check();
        Rejected([&] { HostControlOwnership bad(root,{"localhost"},"bad",digest); });
        Rejected([&] { HostControlOwnership bad(root,{"127.000.0.1"},"bad",digest); });
        Rejected([&] { HostControlOwnership bad(root,{"224.0.0.1"},"bad",digest); });
        const auto symlink=File(root,"127.0.0.7");
        Require(::symlink(File(root,"127.0.0.1").c_str(),symlink.c_str())==0,"symlink setup failed");
        Rejected([&] { HostControlOwnership bad(root,{"127.0.0.7"},"bad",digest); });
        const auto path=File(root,"127.0.0.1");
        const int fd=::open(path.c_str(),O_WRONLY|O_APPEND); Require(fd>=0,"Cannot mutate test reservation");
        Require(::write(fd,"x",1)==1,"Mutation failed"); ::close(fd);
        Rejected([&] { reacquired.Check(); }); Require(reacquired.Faulted(),"Fault was not latched");
        const int restore=::open(path.c_str(),O_WRONLY); Require(restore>=0,"Cannot restore test file");
        struct stat st; Require(::fstat(restore,&st)==0 && ::ftruncate(restore,st.st_size-1)==0,"Restore failed"); ::close(restore);
        Rejected([&] { reacquired.Check(); });
        std::cout<<"PASS ownership conflict/disjoint/rollback/process/reacquire/address/symlink/sticky-fault; no DAQ traffic\n";
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
