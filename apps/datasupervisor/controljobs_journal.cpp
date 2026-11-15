#include "controljobs_journal_p.h"
#include "supervisorio.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;

class SupervisorControlJournal::Impl {
public:
    std::string directory,policy,content;
    std::unique_ptr<Directory> root,operations;
    Fd journal;
    struct stat policyIdentity,journalIdentity;
    bool existing;

    Impl(const std::string &path,const std::string &expectedPolicy)
        : directory(path),policy(expectedPolicy),journal(-1),existing(false) {
        struct stat st;
        existing=::lstat(directory.c_str(),&st)==0;
        if (!existing && errno!=ENOENT) throw std::runtime_error("Cannot inspect supervisor control directory");
        if (!existing) CreateDirectory(directory);
        root.reset(new Directory(directory));
        if (!existing) {
            CreateDirectory(directory+"/operations");
            WriteNew(directory+"/policy.json",policy);
            WriteNew(directory+"/operations.jsonl","");
        }
        operations.reset(new Directory(directory+"/operations"));
        if (ReadFile(directory+"/policy.json",65536)!=policy ||
            ::lstat((directory+"/policy.json").c_str(),&policyIdentity))
            throw std::runtime_error("Immutable supervisor control policy differs");
        journal.value=::open((directory+"/operations.jsonl").c_str(),O_RDWR|O_APPEND|O_NOFOLLOW|O_CLOEXEC);
        if (journal.value<0 || ::fstat(journal.value,&journalIdentity) || !S_ISREG(journalIdentity.st_mode))
            throw std::runtime_error("Invalid supervisor control journal");
        content=ReadFile(directory+"/operations.jsonl",64*1024*1024);
        if (!content.empty() && content.back()!='\n')
            throw std::runtime_error("Incomplete supervisor control journal; no repair/replay");
    }

    void Append(const std::string &record) {
        if (content.size()+record.size()>64*1024*1024)
            throw std::runtime_error("Supervisor control journal budget exhausted");
        std::size_t offset=0;
        while (offset<record.size()) {
            const auto written=::write(journal.value,record.data()+offset,record.size()-offset);
            if (written<0 && errno==EINTR) continue;
            if (written<=0) throw std::runtime_error("Cannot append supervisor control journal");
            offset+=std::size_t(written);
        }
        if (::fsync(journal.value)) throw std::runtime_error("Cannot sync supervisor control journal");
        content+=record;
    }

    void Check() const {
        root->Check();
        operations->Check();
        struct stat current,held;
        if (::lstat((directory+"/policy.json").c_str(),&current) || !Same(current,policyIdentity) ||
            !S_ISREG(current.st_mode) || current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600 ||
            ReadFile(directory+"/policy.json",65536)!=policy)
            throw std::runtime_error("Supervisor control policy identity/content changed");
        if (::lstat((directory+"/operations.jsonl").c_str(),&current) ||
            ::fstat(journal.value,&held) || !Same(current,journalIdentity) || !Same(current,held) ||
            !S_ISREG(current.st_mode) || current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600 ||
            ReadFile(directory+"/operations.jsonl",64*1024*1024)!=content)
            throw std::runtime_error("Supervisor control journal identity/content changed");
    }
};

SupervisorControlJournal::SupervisorControlJournal(const std::string &directory,const std::string &policy)
    : fImpl(new Impl(directory,policy)) {}
SupervisorControlJournal::~SupervisorControlJournal() {}
bool SupervisorControlJournal::Existing() const { return fImpl->existing; }
const std::string &SupervisorControlJournal::Content() const { return fImpl->content; }
void SupervisorControlJournal::Append(const std::string &record) { fImpl->Append(record); }
void SupervisorControlJournal::Check() const { fImpl->Check(); }

}
