#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>

namespace {
std::atomic_flag injected=ATOMIC_FLAG_INIT;
bool FailHandoffProof(const char *path,int flags) noexcept {
    const char *target=std::getenv("GETDAQ_TEST_HANDOFF_PROOF");
    if (!(flags&O_CREAT) || !target || !path) return false;
    const auto targetLength=std::strlen(target),pathLength=std::strlen(path);
    if (!targetLength || pathLength<targetLength || std::strcmp(path+pathLength-targetLength,target)!=0 || injected.test_and_set()) return false;
    errno=EIO;
    return true;
}
}
extern "C" int open(const char *path,int flags,...) noexcept {
    typedef int (*Function)(const char *,int,...);
    static const auto original=reinterpret_cast<Function>(::dlsym(RTLD_NEXT,"open"));
    if (FailHandoffProof(path,flags)) return -1;
    if (!original) { errno=ENOSYS; return -1; }
    if (flags&O_CREAT) { va_list args;va_start(args,flags);const auto mode=va_arg(args,int);va_end(args);return original(path,flags,mode); }
    return original(path,flags);
}
extern "C" int open64(const char *path,int flags,...) noexcept {
    typedef int (*Function)(const char *,int,...);
    static const auto original=reinterpret_cast<Function>(::dlsym(RTLD_NEXT,"open64"));
    if (FailHandoffProof(path,flags)) return -1;
    if (!original) { errno=ENOSYS; return -1; }
    if (flags&O_CREAT) { va_list args;va_start(args,flags);const auto mode=va_arg(args,int);va_end(args);return original(path,flags,mode); }
    return original(path,flags);
}
