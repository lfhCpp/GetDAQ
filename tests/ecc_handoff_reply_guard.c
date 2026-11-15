#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
/* Test-only: consume one actual ECC POST reply, then expose EOF to the CLI.
 * Record only event names, never request headers, tokens or payload contents. */
static int handoff_fd=-1;
static int dropped=0;
static void record(const char *event,size_t size) {
    const char *path=getenv("GETDAQ_HANDOFF_REPLY_LOG");
    if (path) {
        int fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC|O_NOFOLLOW,0600);
        if (fd>=0) {(void)write(fd,event,size);close(fd);}
    }
}
ssize_t send(int fd,const void *buffer,size_t size,int flags) {
    static ssize_t (*original)(int,const void *,size_t,int);
    if (!original) *(void **)(&original)=dlsym(RTLD_NEXT,"send");
    if (!original) return -1;
    ssize_t result=original(fd,buffer,size,flags);
    if (result>0 && handoff_fd<0 && size>24 && !memcmp(buffer,"POST /v1/operations ",20) &&
        memmem(buffer,size,"\"command\":\"handoff-data-control\"",32)) {
        handoff_fd=fd;record("post\n",5);
    }
    return result;
}
ssize_t recv(int fd,void *buffer,size_t size,int flags) {
    static ssize_t (*original)(int,void *,size_t,int);
    if (!original) *(void **)(&original)=dlsym(RTLD_NEXT,"recv");
    if (!original) return -1;
    ssize_t result=original(fd,buffer,size,flags);
    if (fd==handoff_fd && !dropped && result>0) {
        const char *release=getenv("GETDAQ_HANDOFF_REPLY_RELEASE");
        if (release) for (unsigned int i=0;i<500 && access(release,F_OK);++i) usleep(10000);
        dropped=1;record("drop\n",5);return 0;
    }
    return result;
}
