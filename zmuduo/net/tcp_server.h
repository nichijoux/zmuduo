// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TCP_SERVER_H
#define ZMUDUO_NET_TCP_SERVER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/acceptor.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/callbacks.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/event_loop_thread_pool.h"
#include "zmuduo/net/tcp_connection.h"
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace zmuduo::net {
/**
 * @class TcpServer
 * @brief TCP 服务器类，管理监听 socket、事件循环线程池和 TCP 连接。
 *
 * TcpServer 是 Reactor 模型中的核心组件。<br/>
 * 负责监听新连接（通过 Acceptor）、分配 IO 事件循环（通过 EventLoopThreadPool）、管理 TcpConnection
 * 实例。<br/> 处理连接建立、消息收发、连接关闭等事件。<br/> 支持端口复用和 SSL 加密（需启用
 * ZMUDUO_ENABLE_OPENSSL）。
 *
 * @note TcpServer 运行在主事件循环（mainLoop）中，IO 操作由子事件循环（subLoop）处理。
 * @note 所有方法必须在主事件循环线程中调用，以确保线程安全。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Address::Ptr addr = Address::Create("0.0.0.0:8080");
 * TcpServer server(&loop, addr, "TestServer");
 * server.setConnectionCallback([](const TcpConnectionPtr& conn) {
 *     if (conn->isConnected()) {
 *         conn->send("Welcome!");
 *     }
 * });
 * server.setThreadNum(4);
 * server.start();
 * loop.loop();
 * @endcode
 */
class TcpServer : NoCopyable {
  public:
    /**
     * @typedef std::function&lt;void(EventLoop*)&gt;
     * @brief 定义线程初始化回调函数类型，用于配置子事件循环。
     */
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    /**
     * @typedef std::unordered_map&lt;std::string, TcpConnectionPtr&gt;
     * @brief 定义连接映射类型，存储连接名称到 TcpConnection 的映射。
     */
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;

  public:
    /**
     * @brief 构造函数，初始化 TCP 服务器。
     * @param[in] loop 主事件循环。
     * @param[in] listenAddress 监听地址。
     * @param[in] name 服务器名称。
     * @param[in] reusePort 端口复用选项，默认为 false
     */
    TcpServer(EventLoop*          loop,
              const Address::Ptr& listenAddress,
              const std::string&  name,
              bool                reusePort = false);

    /**
     * @brief 析构函数，清理服务器资源。
     * @note 断开所有连接并释放 SSL 上下文（如果启用）。
     */
    ~TcpServer();

    /**
     * @brief 设置事件循环线程池的线程数量。
     * @param[in] num 线程数量（非负整数）。
     * @throw 如果 num 小于 0，抛出断言错误。
     */
    void setThreadNum(int num);

    /**
     * @brief 设置连接建立/断开的回调函数。
     * @param[in] cb 连接回调函数。
     */
    void setConnectionCallback(ConnectionCallback cb) {
        m_connectionCallback = std::move(cb);
    }

    /**
     * @brief 设置接收消息的回调函数。
     * @param[in] cb 消息回调函数。
     */
    void setMessageCallback(MessageCallback cb) {
        m_messageCallback = std::move(cb);
    }

    /**
     * @brief 设置写完成时的回调函数。
     * @param[in] cb 写完成回调函数。
     */
    void setWriteCompleteCallback(WriteCompleteCallback cb) {
        m_writeCompleteCallback = std::move(cb);
    }

    /**
     * @brief 设置线程初始化回调函数。
     * @param[in] cb 线程初始化回调函数。
     */
    void setThreadInitCallback(ThreadInitCallback cb) {
        m_threadInitCallback = std::move(cb);
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    bool loadCertificates(const std::string& certificatePath, const std::string& privateKeyPath);

    SSL_CTX* getSSLContext() const {
        return m_sslContext;
    }
#endif

    /**
     * @brief 启动 TCP 服务器。
     * @note 启动线程池并开始监听新连接；仅当服务器未启动时有效。
     */
    void start();

    /**
     * @brief 服务器是否已经启动
     * @retval true 服务器已经启动
     * @retval false 服务器未启动
     */
    bool isStarted() const {
        return m_started;
    }

    /**
     * @brief 获取主事件循环。
     * @return 主事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_eventLoop;
    }

    /**
     * @brief 获取监听地址的 IP 和端口字符串。
     * @return IP 和端口字符串。
     */
    const std::string& getIpPort() const {
        return m_ipPort;
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
    std::shared_ptr<EventLoopThreadPool> getThreadPool() const {
        return m_threadPool;
    }

  private:
    /**
     * @brief 处理新连接。
     * @param[in] socketFD 新连接的 socket 文件描述符。
     * @param[in] peerAddress 客户端地址。
     * @note 创建 TcpConnection 实例并分配到子事件循环。
     */
    void newConnection(int socketFD, const Address::Ptr& peerAddress);

    /**
     * @brief 移除指定的连接。
     * @param[in] connection 要移除的 TcpConnection 智能指针。
     * @note 异步在主事件循环中执行移除操作。
     */
    void removeConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 在主事件循环中移除连接。
     * @param[in] connection 要移除的 TcpConnection 智能指针。
     * @note 从连接映射中移除并销毁连接。
     */
    void removeConnectionInLoop(const TcpConnectionPtr& connection);

  private:
    EventLoop*                m_eventLoop;              ///< 主事件循环
    const std::string         m_ipPort;                 ///< 监听地址的 IP 和端口
    const std::string         m_name;                   ///< 服务器名称
    std::unique_ptr<Acceptor> m_acceptor;               ///< 监听新连接的 Acceptor
    EventLoopThreadPool::Ptr  m_threadPool;             ///< 事件循环线程池
    ConnectionCallback        m_connectionCallback;     ///< 连接建立/断开回调
    MessageCallback           m_messageCallback;        ///< 消息接收回调
    WriteCompleteCallback     m_writeCompleteCallback;  ///< 写完成回调
    ThreadInitCallback        m_threadInitCallback;     ///< 线程初始化回调
    std::atomic<bool>         m_started;                ///< 服务器是否已启动
    ConnectionMap             m_connections;            ///< 连接名称到 TcpConnection 的映射
    std::atomic<uint64_t>     m_nextConnectId;          ///< 下一个连接的 ID
#ifdef ZMUDUO_ENABLE_OPENSSL
    SSL_CTX* m_sslContext;  ///< SSL 上下文
#endif
};
}  // namespace zmuduo::net

#endif