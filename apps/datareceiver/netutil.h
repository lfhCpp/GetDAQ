#ifndef GETDAQ_NETUTIL_H
#define GETDAQ_NETUTIL_H
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace GetDAQ {
inline void Nonblock(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0 || ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) throw std::runtime_error("Cannot set nonblocking socket");
}
inline void ConfigureNoSigpipe(int fd) {
#if defined(SO_NOSIGPIPE)
    const int enabled = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0)
        throw std::runtime_error(std::string("Cannot configure socket SIGPIPE behavior: ") + std::strerror(errno));
#else
    (void)fd;
#endif
}
inline ssize_t SendNoSigpipe(int fd, const void *data, std::size_t size) {
#if defined(MSG_NOSIGNAL)
    return ::send(fd, data, size, MSG_NOSIGNAL);
#else
    return ::send(fd, data, size, 0);
#endif
}
inline int Listen(const std::string &host, unsigned port) {
    addrinfo hint = {}, *addresses = nullptr;
    hint.ai_family = AF_UNSPEC; hint.ai_socktype = SOCK_STREAM; hint.ai_flags = AI_NUMERICSERV;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hint, &addresses)) throw std::runtime_error("Invalid bind address " + host);
    int result = -1;
    for (addrinfo *a = addresses; a; a = a->ai_next) {
        int fd = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        int yes = 1; ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (::bind(fd, a->ai_addr, a->ai_addrlen) == 0 && ::listen(fd, 8) == 0) { result = fd; break; }
        ::close(fd);
    }
    ::freeaddrinfo(addresses);
    if (result < 0) throw std::runtime_error("Cannot listen on " + host + ":" + std::to_string(port) + ": " + std::strerror(errno));
    try { Nonblock(result); } catch (...) { ::close(result); throw; }
    return result;
}
inline bool Readable(int fd, int ms) {
    pollfd p = {fd, POLLIN, 0};
    const int result = ::poll(&p, 1, ms);
    if (result < 0 && errno != EINTR) throw std::runtime_error("poll failed");
    return result > 0; // HUP/ERR 也交给 recv 解释，不能误当作没有数据。
}
}
#endif
