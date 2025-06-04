#include "zmuduo/net/socket.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/socket_options.h"
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>

namespace zmuduo::net {
Socket::~Socket() {
    if (m_fd != -1) {
        ::close(m_fd);
    }
}

void Socket::bind(const Address::Ptr& localAddress) const {
    if (::bind(m_fd, localAddress->getSockAddress(), localAddress->getSockAddressLength()) != 0) {
        ZMUDUO_LOG_FMT_FATAL("bind socket %d error", m_fd);
    }
}

void Socket::setReuseAddress(bool on) const {
#ifdef SO_REUSEADDR
    int optval = on ? 1 : 0;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEADDR, &optval, static_cast<socklen_t>(sizeof(optval)));
#else
    if (on) {
        LOG_ERROR("SO_REUSEADDR is not supported.");
    }
#endif
}

void Socket::setReusePort(bool on) const {
#ifdef SO_REUSEPORT
    int optval = on ? 1 : 0;
    setsockopt(m_fd, SOL_SOCKET, SO_REUSEPORT, &optval, static_cast<socklen_t>(sizeof(optval)));
#else
    if (on) {
        LOG_ERROR("SO_REUSEPORT is not supported.");
    }
#endif
}

int Socket::CreateNonBlockingSocket(int domain, int type, int protocol) {
#if VALGRIND
    int fd = ::socket(domain, type, protocol);
    if (fd < 0) {
        ZMUDUO_LOG_FMT_FATAL("create socket failed: %d", errno);
    }
    sockets::setNonBlockAndCloseOnExec(fd);
#else
    int fd = ::socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
    if (fd < 0) {
        ZMUDUO_LOG_FMT_FATAL("create socket failed: %d", errno);
    }
#endif
    return fd;
}

void TcpSocket::listen() const {
    if (::listen(m_fd, SOMAXCONN) != 0) {
        ZMUDUO_LOG_FMT_FATAL("listen socket %d error", m_fd);
    }
}

int TcpSocket::accept(Address::Ptr& peerAddress) {
    sockaddr_in addr{};
    socklen_t   length = sizeof(addr);
    bzero(&addr, sizeof(addr));
#if VALGRIND || defined(NO_ACCEPT4)
    int remoteFD = ::accept(m_fd, const_cast<sockaddr*>(sockets::sockaddr_cast(&addr)), &length);
    sockets::setNonBlockAndCloseOnExec(remoteFD);
#else
    int remoteFD = ::accept4(m_fd, const_cast<sockaddr*>(sockets::sockaddr_cast(&addr)), &length,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif
    if (remoteFD >= 0) {
        peerAddress = Address::Create(sockets::sockaddr_cast(&addr));
    } else {
        switch (errno) {
            case EAGAIN:
            case ECONNABORTED:
            case EINTR:
            case EPROTO:  // ???
            case EPERM:
            case EMFILE:  // per-process lmit of open file desctiptor ???
                break;
            case EBADF:
            case EFAULT:
            case EINVAL:
            case ENFILE:
            case ENOBUFS:
            case ENOMEM:
            case ENOTSOCK:
            case EOPNOTSUPP:
                // unexpected errors
                ZMUDUO_LOG_FMT_FATAL("unexpected error of ::accept %d", errno);
            default: ZMUDUO_LOG_FMT_FATAL("unknown error of ::accept %d", errno);
        }
    }
    return remoteFD;
}

void TcpSocket::shutdownWrite() const {
    if (::shutdown(m_fd, SHUT_WR) != 0) {
        ZMUDUO_LOG_FMT_ERROR("shutdown socket %d error", m_fd);
    }
}

void TcpSocket::setKeepAlive(bool on) const {
#ifdef SO_KEEPALIVE
    int optval = on ? 1 : 0;
    setsockopt(m_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, static_cast<socklen_t>(sizeof(optval)));
#else
    if (on) {
        LOG_ERROR("SO_KEEPALIVE is not supported.");
    }
#endif
}

void TcpSocket::setTcpNoDelay(bool on) const {
#ifdef TCP_NODELAY
    int optval = on ? 1 : 0;
    setsockopt(m_fd, IPPROTO_TCP, TCP_NODELAY, &optval, static_cast<socklen_t>(sizeof(optval)));
#else
    if (on) {
        LOG_ERROR("TCP_NODELAY is not supported.");
    }
#endif
}

bool TcpSocket::getTcpInfo(tcp_info* info) const {
    socklen_t len = sizeof(*info);
    bzero(info, len);
    return ::getsockopt(m_fd, SOL_TCP, TCP_INFO, info, &len) == 0;
}
}  // namespace zmuduo::net