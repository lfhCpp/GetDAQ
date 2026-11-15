#define _GNU_SOURCE
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
// 仅测试LD_PRELOAD库：只影响明确私有输出目录，产品不链接或加载。
static _Atomic int once=0;
static int target(int fd,int directory,const char *suffix) {
    const char *root=getenv("GETDAQ_TEST_IO_DIRECTORY");if(!root || root[0]!='/') return 0;
    char link[64],path[PATH_MAX+64];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);
    ssize_t n=readlink(link,path,sizeof(path)-1);if(n<0) return 0;path[n]=0;
    if(directory) return strcmp(path,root)==0;
    size_t length=strlen(root),ending=strlen(suffix);
    return strncmp(path,root,length)==0 && path[length]=='/' && (size_t)n>=ending && strcmp(path+n-ending,suffix)==0;
}
static int mode(const char *name) {const char *value=getenv("GETDAQ_TEST_IO_MODE");return value && strcmp(value,name)==0;}
ssize_t write(int fd,const void *data,size_t count) {
    if(target(fd,0,".graw")) {
        if(mode("write-zero")) {errno=0;return 0;}
        if(mode("write-eio")) {errno=EIO;return -1;}
        if(mode("write-eintr-once")) {int expected=0;if(atomic_compare_exchange_strong(&once,&expected,1)) {errno=EINTR;return -1;}}
    }
    return syscall(SYS_write,fd,data,count);
}
int fsync(int fd) {
    int fail=(mode("fsync-raw-eio") && target(fd,0,".graw")) ||
             (mode("fsync-directory-eio") && target(fd,1,"")) ||
             (mode("fsync-seal-eio") && target(fd,0,".getdaq-receiver-seal.json.tmp"));
    if(mode("fsync-published-directory-eio") && target(fd,1,"")) {
        const char *root=getenv("GETDAQ_TEST_IO_DIRECTORY");char name[PATH_MAX+64];
        snprintf(name,sizeof(name),"%s/.getdaq-receiver-seal.json",root);if(access(name,F_OK)==0) fail=1;
    }
    if(fail) {errno=EIO;return -1;}
    return (int)syscall(SYS_fsync,fd);
}
int rename(const char *before,const char *after) {
    const char *root=getenv("GETDAQ_TEST_IO_DIRECTORY");
    if(root && mode("rename-seal-eperm")) {
        char target_name[PATH_MAX+64];snprintf(target_name,sizeof(target_name),"%s/.getdaq-receiver-seal.json",root);
        if(strcmp(after,target_name)==0) {errno=EPERM;return -1;}
    }
    return (int)syscall(SYS_rename,before,after);
}
int close(int fd) {
    const int fail=mode("close-seal-eio") && target(fd,0,".getdaq-receiver-seal.json.tmp");
    const int result=(int)syscall(SYS_close,fd);
    if(fail && result==0) {errno=EIO;return -1;} // Linux关闭后错误，不再次关闭这个fd。
    return result;
}
