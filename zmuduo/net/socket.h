// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_SOCKET_H
#define ZMUDUO_NET_SOCKET_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/address.h"
#include <memory>
#include <netinet/tcp.h>

namespace zmuduo::net {
/**
 * @class Socket
 * @brief 通用 socket 封装基类。
 *
 * Socket 提供对底层 socket 的通用操作，包括创建、绑定、设置选项等。<br/>
 * 支持 IPv4/IPv6 和多种协议（TCP/UDP），不可拷贝（继承自 NoCopyable）。<br/>
 * 用作 TcpSocket 和 UdpSocket 的基类，支撑网络通信底层功能。
 *
 * @note Socket 自动管理文件描述符（FD），析构时关闭 FD。
 * @note 调用者需确保 socket 操作的线程安全。
 *
 * @example
 *
 * @code
 * int fd = Socket::CreateNonBlockingSocket(AF_INET, SOCK_STREAM, 0);
 * TcpSocket socket(fd);
 * socket.setReuseAddress(true);
 * Address::Ptr addr = Address::Create("0.0.0.0", 8080);
 * socket.bind(addr);
 * @endcode
 */
class Socket : NoCopyable {
  public:
    /**
     * @typedef std::unique_ptr&lt;Socket&gt;
     * @brief Socket 智能指针类型。
     */
    using Ptr = std::unique_ptr<Socket>;

  public:
    /**
     * @brief 析构函数。
     * @note 关闭 socket 文件描述符（若有效）。
     */
    ~Socket();

    /**
     * @brief 获取 socket 文件描述符。
     * @return socket 文件描述符（FD）。
     */
    int getFD() const {
        return m_fd;
    }

    /**
     * @brief 绑定 socket 到本地地址。
     * @param[in] localAddress 本地地址（包含 IP 和端口）。
     * @throw std::runtime_error 如果绑定失败，记录致命错误日志。
     */
    void bind(const Address::Ptr& localAddress) const;

    /**
     * @brief 设置 SO_REUSEADDR 选项。
     * @param[in] on 是否启用地址重用。
     * @note 启用后允许绑定到已在使用的地址（若支持 SO_REUSEADDR）。
     */
    void setReuseAddress(bool on) const;

    /**
     * @brief 设置 SO_REUSEPORT 选项。
     * @param[in] on 是否启用端口重用。
     * @note 启用后允许多个 socket 绑定到同一端口（若支持 SO_REUSEPORT）。
     */
    void setReusePort(bool on) const;

  protected:
    /**
     * @brief 创建非阻塞 socket。
     * @param[in] domain 协议族（如 AF_INET, AF_INET6）。
     * @param[in] type socket 类型（如 SOCK_STREAM, SOCK_DGRAM）。
     * @param[in] protocol 协议（如 IPPROTO_TCP, IPPROTO_UDP）。
     * @return 创建的 socket 文件描述符（FD）。
     * @throw std::runtime_error 如果创建失败，记录致命错误日志。
     */
    static int CreateNonBlockingSocket(int domain, int type, int protocol);

  protected:
    int m_fd;  ///< socket 文件描述符
};

/**
 * @class TcpSocket
 * @brief TCP socket 封装类。
 *
 * TcpSocket 继承自 Socket，提供 TCP 特定功能，包括监听、接受连接、关闭写端、设置 TCP 选项等。<br/>
 * 用于实现 TCP 服务器或客户端通信，支撑 HttpServer 等上层组件。
 *
 * @note TcpSocket 不可拷贝，需通过智能指针（Ptr）管理。
 *
 * @example
 *
 * @code
 * TcpSocket socket = TcpSocket::Create(AF_INET);
 * socket.setReuseAddress(true);
 * Address::Ptr addr = Address::Create("0.0.0.0", 8080);
 * socket.bind(addr);
 * socket.listen();
 * Address::Ptr peerAddr;
 * int clientFD = socket.accept(peerAddr);
 * @endcode
 */
class TcpSocket : public Socket {
  public:
    /**
     * @typedef std::unique_ptr&lt;TcpSocket&gt;
     * @brief TcpSocket 智能指针类型。
     */
    using Ptr = std::unique_ptr<TcpSocket>;

    /**
     * @brief 创建 TCP socket。
     * @param[in] domain 协议族（如 AF_INET, AF_INET6）。
     * @return 新创建的 TcpSocket 对象。
     * @note 调用 CreateNonBlockingSocket 创建非阻塞 TCP socket。
     */
    static TcpSocket Create(int domain) {
        int fd = Socket::CreateNonBlockingSocket(domain, SOCK_STREAM, IPPROTO_TCP);
        return TcpSocket(fd);
    }

  public:
    /**
     * @brief 构造函数。
     * @param[in] fd socket 文件描述符。
     * @note 初始化为 TCP socket
     */
    explicit TcpSocket(int fd) : Socket() {
        m_fd = fd;
    }

    /**
     * @brief 监听 socket，准备接受连接。
     * @throw std::runtime_error 如果监听失败，记录致命错误日志。
     */
    void listen() const;

    /**
     * @brief 接受客户端连接。
     * @param[out] peerAddress 客户端地址（输出参数）。
     * @return 客户端 socket 文件描述符（成功）或 -1（失败）。
     * @note 成功时设置 peerAddress，失败时记录错误日志（特定错误可能不记录）。
     */
    int accept(Address::Ptr& peerAddress);

    /**
     * @brief 关闭 socket 的写端。
     * @note 用于半关闭连接，通知对端本地不再发送数据。
     * @throw std::runtime_error 如果关闭失败，记录错误日志。
     */
    void shutdownWrite() const;

    /**
     * @brief 设置 SO_KEEPALIVE 选项。
     * @param[in] on 是否启用 TCP 保活机制。
     * @note 启用后检测连接是否存活（若支持 SO_KEEPALIVE）。
     */
    void setKeepAlive(bool on) const;

    /**
     * @brief 设置 TCP_NODELAY 选项。
     * @param[in] on 是否启用 Nagle 算法禁用。
     * @note 启用后减少小包延迟（若支持 TCP_NODELAY）。
     */
    void setTcpNoDelay(bool on) const;

    /**
     * @brief 获取 TCP 连接信息。
     * @param[out] info TCP 连接信息结构体。
     * @retval true 获取信息成功，info 包含有效数据。
     * @retval false 获取信息失败，info 未修改。
     */
    bool getTcpInfo(tcp_info* info) const;
};

/**
 * @class UdpSocket
 * @brief UDP socket 封装类。
 *
 * UdpSocket 继承自 Socket，提供 UDP 特定功能，包括发送和接收数据报。<br/>
 * 用于实现无连接的 UDP 通信场景。
 *
 * @note UdpSocket 不可拷贝，需通过智能指针（Ptr）管理。
 *
 * @example
 *
 * @code
 * UdpSocket socket = UdpSocket::Create(AF_INET);
 * Address::Ptr addr = Address::Create("127.0.0.1", 12345);
 * const char* msg = "Hello, UDP!";
 * socket.sendTo(msg, strlen(msg), addr);
 * char buffer[1024];
 * Address::Ptr peerAddr;
 * ssize_t n = socket.receiveFrom(buffer, sizeof(buffer), peerAddr);
 * @endcode
 */
class UdpSocket : public Socket {
  public:
    /**
     * @typedef std::unique_ptr&lt;UdpSocket&gt;
     * @brief UdpSocket 智能指针类型。
     */
    using Ptr = std::unique_ptr<UdpSocket>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] fd socket 文件描述符。
     * @note 初始化为 UDP socket，设置类型为 SOCK_DGRAM，协议为 IPPROTO_UDP，域为未知（-1）。
     */
    explicit UdpSocket(int fd) : Socket() {
        m_fd = fd;
    }

    /**
     * @brief 创建 UDP socket。
     * @param[in] domain 协议族（如 AF_INET, AF_INET6）。
     * @return 新创建的 UdpSocket 对象。
     * @note 调用 CreateNonBlockingSocket 创建非阻塞 UDP socket。
     */
    static UdpSocket Create(int domain) {
        int fd = Socket::CreateNonBlockingSocket(domain, SOCK_DGRAM, IPPROTO_UDP);
        return UdpSocket(fd);
    }
};
}  // namespace zmuduo::net

#endif