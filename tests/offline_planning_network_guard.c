/* Test-only interposer: fail and record any attempt to connect/bind/listen. */
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
static int deny_network(const char *name) {
    const char *path=getenv("GETDAQ_OFFLINE_NETWORK_LOG");
    if (path) {
        int fd=open(path,O_WRONLY|O_CREAT|O_APPEND,0600);
        if (fd>=0) { (void)write(fd,name,strlen(name)); (void)write(fd,"\n",1); close(fd); }
    }
    errno=EPERM;
    return -1;
}
int connect(int fd,const struct sockaddr *address,socklen_t size) {
    (void)fd;(void)address;(void)size;return deny_network("connect");
}
int bind(int fd,const struct sockaddr *address,socklen_t size) {
    (void)fd;(void)address;(void)size;return deny_network("bind");
}
int listen(int fd,int backlog) {
    (void)fd;(void)backlog;return deny_network("listen");
}
ssize_t sendto(int fd,const void *bytes,size_t count,int flags,const struct sockaddr *address,socklen_t size) {
    (void)fd;(void)bytes;(void)count;(void)flags;(void)address;(void)size;return deny_network("sendto");
}
ssize_t sendmsg(int fd,const struct msghdr *message,int flags) {
    (void)fd;(void)message;(void)flags;return deny_network("sendmsg");
}
