// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_UDP_SERVER_H
#define ZMUDUO_NET_UDP_SERVER_H

#include "zmuduo/net/address.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/event_loop_thread_pool.h"
#include "zmuduo/net/socket.h"
#include <functional>

namespace zmuduo::net {
/**
 * @class UdpServer
 * @brief UDP 服务器核心类。
 *
 * UdpServer 基于事件驱动实现 UDP 服务器，负责监听和处理 UDP 数据报。<br/>
 * 使用 UdpSocket 进行数据收发，通过 MessageCallback 提供自定义消息处理逻辑，<br/>
 * 支持多线程处理（通过 EventLoopThreadPool）。适用于高性能 UDP 通信场景。
 *
 * @note UdpServer 不可拷贝（继承自 NoCopyable），通过 EventLoop 管理异步 I/O。
 * @note 调用者需确保所有操作在正确的 EventLoop 线程中执行，回调函数需线程安全。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Address::Ptr addr = Address::Create("0.0.0.0", 12345);
 * UdpServer server(&loop, addr, "MyUdpServer");
 * server.setMessageCallback([](Buffer& input, const Address::Ptr& peer, Buffer& output) {
 *     std::string msg(input.peek(), input.getReadableBytes());
 *     output.append("Echo: " + msg);
 * });
 * server.setThreadNum(4);
 * server.start();
 * loop.loop();
 * @endcode
 */
class UdpServer : NoCopyable {
  public:
    /**
     * @typedef std::function&lt;void(Buffer& inputBuffer, const Address::Ptr& peerAddress, Buffer&
     * outputBuffer)&gt;
     * @brief 消息回调函数类型。
     * @param[in] server udp服务器
     * @param[in,out] buffer 输入缓冲区，包含接收到的数据。
     * @param[in] peerAddress 来源地址
     */
    using MessageCallback =
        std::function<void(UdpServer& server, Buffer& buffer, const Address::Ptr& peerAddress)>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] listenAddress 监听地址（如 "0.0.0.0:12345"）。
     * @param[in] name 服务器名称，用于日志记录。
     * @note 初始化 UdpSocket，绑定地址，设置 Channel 读回调。
     */
    UdpServer(EventLoop* loop, const Address::Ptr& listenAddress, const std::string& name);

    /**
     * @brief 析构函数。
     * @note 确保在 EventLoop 线程中销毁，记录调试日志。
     */
    ~UdpServer();

    /**
     * @brief 启动服务器。
     * @note 启用 Channel 读事件，开始监听 UDP 数据报。使用原子操作确保只启动一次。
     */
    void start();

    /**
     * @brief 设置事件循环线程池的线程数量。
     * @param[in] num 线程数量（非负整数）。
     * @throw std::runtime_error 如果 num 小于 0，抛出断言错误。
     * @note 必须在 start() 前调用。
     */
    void setThreadNum(int num);

    /**
     * @brief 设置接收消息的回调函数。
     * @param[in] cb 消息回调函数。
     * @note 回调在 I/O 线程中执行，需确保线程安全。
     */
    void setMessageCallback(MessageCallback cb) {
        m_messageCallback = std::move(cb);
    }

    /**
     * @brief 向指定地址发送字符串数据。
     * @param[in] data 字符串数据。
     * @param[in] peerAddress 目标地址。
     * @note 委托给 send(const void*, size_t, const Address::Ptr&)。
     */
    void send(const std::string& data, const Address::Ptr& peerAddress) {
        send(data.c_str(), data.size(), peerAddress);
    }

    /**
     * @brief 向指定地址发送数据。
     * @param[in] data 数据指针。
     * @param[in] length 数据长度。
     * @param[in] peerAddress 目标地址。
     * @note 异步调用 sendInLoop，通过线程池分发。
     */
    void send(const void* data, size_t length, const Address::Ptr& peerAddress);
    /**
     * @brief 获取主事件循环。
     * @return 主事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_eventLoop;
    }

    /**
     * @brief 获取服务器名称。
     * @return 服务器名称。
     */
    const std::string& getName() const {
        return m_name;
    }

    /**
     * @brief 获取事件循环线程池。
     * @return 线程池智能指针。
     */
    EventLoopThreadPool::Ptr getThreadPool() const {
        return m_threadPool;
    }

  private:
    /**
     * @brief 在事件循环线程中发送数据。
     * @param[in] data 数据指针。
     * @param[in] length 数据长度。
     * @param[in] peerAddress 目标地址。
     * @note 使用 sendto 发送数据。
     */
    void sendInLoop(const void* data, size_t length, const Address::Ptr& peerAddress);

    /**
     * @brief 处理读事件。
     * @note 分配到线程池的下一个 EventLoop，调用 newMessage 处理消息。
     */
    void handleRead();

    /**
     * @brief 处理新消息。
     * @note 接收 UDP 数据报，调用 MessageCallback 生成响应，并发送响应。
     */
    void newMessage();

  private:
    EventLoop*               m_eventLoop;        ///< 主事件循环
    const std::string        m_ipPort;           ///< 监听地址（IP:Port 格式）
    const std::string        m_name;             ///< 服务器名称
    UdpSocket                m_socket;           ///< UDP socket
    Channel                  m_channel;          ///< socket 事件通道
    EventLoopThreadPool::Ptr m_threadPool;       ///< 事件循环线程池
    MessageCallback          m_messageCallback;  ///< 消息处理回调
    std::atomic<bool>        m_started;          ///< 服务器是否已启动
};
}  // namespace zmuduo::net

#endif