#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* Test-only I/O boundary: supervisors may stat .graw, never read its bytes. */
static int raw(const char *path) {size_t n=path ? strlen(path) : 0;return n>=5 && !strcmp(path+n-5,".graw");}
static void denied(void) {
    const char *path=getenv("GETDAQ_RAW_READ_LOG");
    if (path) {int fd=open(path,O_WRONLY|O_CREAT|O_APPEND|O_CLOEXEC,0600);if(fd>=0){(void)write(fd,"raw-read-denied\n",16);close(fd);}}
    errno=EPERM;
}
static int reading(int flags) {return (flags&O_ACCMODE)!=O_WRONLY;}
static int needs_mode(int flags) {
    int value=(flags&O_CREAT)!=0;
#ifdef O_TMPFILE
    value=value || (flags&O_TMPFILE)==O_TMPFILE;
#endif
    return value;
}
#define OPEN_WRAPPER(name) \
int name(const char *path,int flags,...) { \
    static int (*original)(const char *,int,...);mode_t mode=0; \
    if(needs_mode(flags)){va_list args;va_start(args,flags);mode=va_arg(args,int);va_end(args);} \
    if(raw(path)&&reading(flags)){denied();return -1;} \
    if(!original)*(void **)(&original)=dlsym(RTLD_NEXT,#name); \
    if(!original){errno=ENOSYS;return -1;}return original(path,flags,mode); \
}
OPEN_WRAPPER(open)
OPEN_WRAPPER(open64)
#define OPENAT_WRAPPER(name) \
int name(int root,const char *path,int flags,...) { \
    static int (*original)(int,const char *,int,...);mode_t mode=0; \
    if(needs_mode(flags)){va_list args;va_start(args,flags);mode=va_arg(args,int);va_end(args);} \
    if(raw(path)&&reading(flags)){denied();return -1;} \
    if(!original)*(void **)(&original)=dlsym(RTLD_NEXT,#name); \
    if(!original){errno=ENOSYS;return -1;}return original(root,path,flags,mode); \
}
OPENAT_WRAPPER(openat)
OPENAT_WRAPPER(openat64)
#define FOPEN_WRAPPER(name) \
FILE *name(const char *path,const char *mode) { \
    static FILE *(*original)(const char *,const char *); \
    if(raw(path)&&(strchr(mode,'r')||strchr(mode,'+'))){denied();return NULL;} \
    if(!original)*(void **)(&original)=dlsym(RTLD_NEXT,#name); \
    if(!original){errno=ENOSYS;return NULL;}return original(path,mode); \
}
FOPEN_WRAPPER(fopen)
FOPEN_WRAPPER(fopen64)
