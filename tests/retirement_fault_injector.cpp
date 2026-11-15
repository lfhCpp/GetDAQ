#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace {
std::atomic_flag injected=ATOMIC_FLAG_INIT;
void MoveReviewedPlan(const char *path) noexcept {
    const char *target=std::getenv("GETDAQ_TEST_RETIREMENT_PLAN");
    const char *marker=std::getenv("GETDAQ_TEST_RETIREMENT_MARKER");
    const char *backup=std::getenv("GETDAQ_TEST_RETIREMENT_BACKUP");
    if(!target || !marker || !backup || std::strcmp(path,target)) return;
    struct stat st;
    const char *arm=std::getenv("GETDAQ_TEST_RETIREMENT_ARM");
    if(arm && ::syscall(SYS_newfstatat,AT_FDCWD,arm,&st,0))return;
    // For the Qt integration path, the fixture publishes this marker after
    // the child request is durably accepted. A short bounded wait closes the
    // journal-observer race; pre-acceptance plan reads still see the file.
    const char *waitText=std::getenv("GETDAQ_TEST_RETIREMENT_MARKER_WAIT_MS");
    char *end=nullptr;const long waitMs=waitText ? std::strtol(waitText,&end,10) : 0;
    const long boundedWait=(end && !*end && waitMs>0) ? (waitMs>5000 ? 5000 : waitMs) : 0;
    long attempt=0;
    while(::syscall(SYS_newfstatat,AT_FDCWD,marker,&st,0)) {
        if(attempt++>=boundedWait)return;
        const struct timespec pause={0,1000000};::nanosleep(&pause,nullptr);
    }
    if(injected.test_and_set())return;
    // Only the private fixture's exact reviewed path; preserve the original inode.
    ::syscall(SYS_renameat2,AT_FDCWD,target,AT_FDCWD,backup,1 /* RENAME_NOREPLACE */);
}
}
extern "C" int __lxstat(int version,const char *path,struct stat *value) noexcept {
    typedef int (*Function)(int,const char *,struct stat *);
    static const auto original=reinterpret_cast<Function>(::dlsym(RTLD_NEXT,"__lxstat"));
    MoveReviewedPlan(path);if(!original){errno=ENOSYS;return -1;}return original(version,path,value);
}
extern "C" int lstat(const char *path,struct stat *value) noexcept {
    typedef int (*Function)(const char *,struct stat *);
    static const auto original=reinterpret_cast<Function>(::dlsym(RTLD_NEXT,"lstat"));
    MoveReviewedPlan(path);if(!original){errno=ENOSYS;return -1;}return original(path,value);
}
