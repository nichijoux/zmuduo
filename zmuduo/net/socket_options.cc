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

int createNonblockingOrDie(const sa_family_t family) {
#if VALGRIND
    int socketFD = ::socket(family, SOCK_STREAM, IPPROTO_TCP);
    if (socketFD < 0) {
        LOG_FATAL("sockets::createNonblockingOrDie");
    }

    setNonBlockAndCloseOnExec(socketFD);
#else

    const int sockFD = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockFD < 0) {
        ZMUDUO_LOG_FMT_FATAL("sockets::createNonblockingOrDie");
    }
#endif
    return sockFD;
}

int getSocketError(const int sockFD) {
    int  optVal;
    auto optLen = static_cast<socklen_t>(sizeof(optVal));

    if (::getsockopt(sockFD, SOL_SOCKET, SO_ERROR, &optVal, &optLen) < 0) {
        return errno;
    }
    return optVal;
}

const sockaddr* sockaddr_cast(const sockaddr_in* addr) {
    return static_cast<const sockaddr*>(reinterpret_cast<const void*>(addr));
}

const sockaddr* sockaddr_cast(const sockaddr_in6* addr) {
    return static_cast<const sockaddr*>(reinterpret_cast<const void*>(addr));
}

const sockaddr_in* sockaddr_in_cast(const sockaddr* addr) {
    return static_cast<const sockaddr_in*>(reinterpret_cast<const void*>(addr));
}

const sockaddr_in6* sockaddr_in6_cast(const sockaddr* addr) {
    return static_cast<const sockaddr_in6*>(reinterpret_cast<const void*>(addr));
}

sockaddr_in getLocalAddress(const int sockFD) {
    sockaddr_in address{};
    auto        length = static_cast<socklen_t>(sizeof(address));
    bzero(&address, length);
    getsockname(sockFD, reinterpret_cast<sockaddr*>(&address), &length);
    return address;
}

sockaddr_in getPeerAddress(const int sockFD) {
    sockaddr_in address{};
    auto        length = static_cast<socklen_t>(sizeof(address));
    bzero(&address, length);
    getpeername(sockFD, reinterpret_cast<sockaddr*>(&address), &length);
    return address;
}

bool isSelfConnect(const int sockFD) {
    auto localAddress = getLocalAddress(sockFD);
    auto peerAddress  = getPeerAddress(sockFD);
    if (localAddress.sin_family == AF_INET) {
        // ipv4
        const auto localAddressIn = reinterpret_cast<const sockaddr_in*>(&localAddress);
        const auto peerAddressIn  = reinterpret_cast<const sockaddr_in*>(&peerAddress);
        return localAddressIn->sin_port == peerAddressIn->sin_port &&
               localAddressIn->sin_addr.s_addr == peerAddressIn->sin_addr.s_addr;
    }
    if (localAddress.sin_family == AF_INET6) {
        // ipv6
        const auto localAddressIn = reinterpret_cast<sockaddr_in6*>(&localAddress);
        const auto peerAddressIn  = reinterpret_cast<sockaddr_in6*>(&peerAddress);
        return localAddressIn->sin6_port == peerAddressIn->sin6_port &&
               memcmp(&localAddressIn->sin6_addr, &peerAddressIn->sin6_addr,
                      sizeof(localAddressIn->sin6_addr)) == 0;
    }
    return false;
}
} // namespace zmuduo::net::sockets