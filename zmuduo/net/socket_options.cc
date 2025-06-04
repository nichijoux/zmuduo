#include "zmuduo/net/socket_options.h"
#include "zmuduo/base/logger.h"
#include <cstring>
#include <strings.h>

namespace zmuduo::net::sockets {
#if VALGRIND || defined(NO_ACCEPT4)
#include <fcntl.h>

void setNonBlockAndCloseOnExec(int socketFD) {
    // 设置非阻塞模式
    // 获取文件描述符的当前标志
    int flags = fcntl(socketFD, F_GETFL, 0);
    // 将非阻塞标志位设置为1，通过按位或操作添加O_NONBLOCK标志
    flags |= O_NONBLOCK;
    // 设置文件描述符的新标志
    int ret = fcntl(socketFD, F_SETFL, flags);
    // 错误处理
    if (ret != 0) {
        ZMUDUO_LOG_FATAL << "fcntl error";
    }
    // 设置在执行exec系列函数时自动关闭文件描述符
    // 获取文件描述符的文件描述符标志
    flags = fcntl(socketFD, F_GETFD, 0);
    // 将FD_CLOEXEC标志位设置为1，通过按位或操作添加FD_CLOEXEC标志
    flags |= FD_CLOEXEC;
    // 设置文件描述符的新文件描述符标志
    ret = fcntl(socketFD, F_SETFD, flags);
    if (ret != 0) {
        ZMUDUO_LOG_FATAL << "fcntl error";
    }
}
#endif

int createNonblockingOrDie(sa_family_t family) {
#if VALGRIND
    int socketFD = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (socketFD < 0) {
        LOG_FATAL("sockets::createNonblockingOrDie");
    }

    setNonBlockAndCloseOnExec(socketFD);
#else

    int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd < 0) {
        ZMUDUO_LOG_FMT_FATAL("sockets::createNonblockingOrDie");
    }
#endif
    return sockfd;
}

int getSocketError(int sockfd) {
    int  optval;
    auto optlen = static_cast<socklen_t>(sizeof(optval));

    if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        return errno;
    } else {
        return optval;
    }
}

const sockaddr* sockaddr_cast(const sockaddr_in* addr) {
    return static_cast<const sockaddr*>(implicit_cast<const void*>(addr));
}

const sockaddr* sockaddr_cast(const sockaddr_in6* addr) {
    return static_cast<const sockaddr*>(implicit_cast<const void*>(addr));
}

const sockaddr_in* sockaddr_in_cast(const sockaddr* addr) {
    return static_cast<const sockaddr_in*>(implicit_cast<const void*>(addr));
}

const sockaddr_in6* sockaddr_in6_cast(const sockaddr* addr) {
    return static_cast<const sockaddr_in6*>(implicit_cast<const void*>(addr));
}

sockaddr_in getLocalAddress(int sockfd) {
    sockaddr_in address{};
    auto        length = static_cast<socklen_t>(sizeof(address));
    bzero(&address, length);
    getsockname(sockfd, reinterpret_cast<sockaddr*>(&address), &length);
    return address;
}

sockaddr_in getPeerAddress(int sockfd) {
    sockaddr_in address{};
    auto        length = static_cast<socklen_t>(sizeof(address));
    bzero(&address, length);
    getpeername(sockfd, reinterpret_cast<sockaddr*>(&address), &length);
    return address;
}

bool isSelfConnect(int sockFD) {
    auto localAddress = getLocalAddress(sockFD);
    auto peerAddress  = getPeerAddress(sockFD);
    if (localAddress.sin_family == AF_INET) {
        // ipv4
        auto localAddressIn = reinterpret_cast<const sockaddr_in*>(&localAddress);
        auto peerAddressIn  = reinterpret_cast<const sockaddr_in*>(&peerAddress);
        return localAddressIn->sin_port == peerAddressIn->sin_port &&
               localAddressIn->sin_addr.s_addr == peerAddressIn->sin_addr.s_addr;
    } else if (localAddress.sin_family == AF_INET6) {
        // ipv6
        auto localAddressIn = reinterpret_cast<sockaddr_in6*>(&localAddress);
        auto peerAddressIn  = reinterpret_cast<sockaddr_in6*>(&peerAddress);
        return localAddressIn->sin6_port == peerAddressIn->sin6_port &&
               memcmp(&localAddressIn->sin6_addr, &peerAddressIn->sin6_addr,
                      sizeof(localAddressIn->sin6_addr)) == 0;
    }
    return false;
}
}  // namespace zmuduo::net::sockets