// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TCP_CONNECTION_H
#define ZMUDUO_NET_TCP_CONNECTION_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/callbacks.h"
#include "zmuduo/net/socket.h"
#include <any>
#include <memory>
#include <netinet/tcp.h>

#ifdef ZMUDUO_ENABLE_OPENSSL
#    include <openssl/ssl.h>
#endif

namespace zmuduo::net {
class Channel;
class EventLoop;
class Socket;

/**
 * @class TcpConnection
 * @brief 表示一个 TCP 连接，管理 socket、缓冲区、回调和可选的 SSL 加密。
 *
 * TcpConnection 是 Reactor 模型中的核心组件。<br/>
 * 负责处理 TCP 连接的建立、数据收发、状态管理和连接关闭。<br/>
 * 通过 Channel 与 Poller 交互，触发读写事件。<br/>
 * 通过 Buffer 管理输入输出数据；支持用户自定义回调和上下文。
 *
 * @note TcpConnection 必须在事件循环线程中操作，线程安全由 EventLoop 保证。
 * @note 支持高水位标记机制，防止发送缓冲区无限增长。
 * @note 支持 SSL 加密（需启用 ZMUDUO_ENABLE_OPENSSL）。
 *
 * @example
 *
 * @code
 * void onConnection(const TcpConnectionPtr& conn) {
 *     if (conn->isConnected()) {
 *         conn->send("Hello, world!");
 *     }
 * }
 * TcpConnection conn(loop, "conn1", sockfd, localAddr, peerAddr);
 * conn.setConnectionCallback(onConnection);
 * conn.connectEstablished();
 * @endcode
 */
class TcpConnection : NoCopyable, public std::enable_shared_from_this<TcpConnection> {
public:
    /**
     * @brief 构造函数，初始化 TCP 连接。
     * @param[in] loop 所属的事件循环。
     * @param[in] name 连接名称。
     * @param[in] socketFD 连接的 socket 文件描述符。
     * @param[in] localAddress 本地地址智能指针。
     * @param[in] peerAddress 远程地址智能指针。
     * @param[in] ssl SSL 上下文指针（仅当启用 ZMUDUO_ENABLE_OPENSSL 时有效）。
     */
    TcpConnection(EventLoop*   loop,
                  std::string  name,
                  int          socketFD,
                  Address::Ptr localAddress,
                  Address::Ptr peerAddress
#ifdef ZMUDUO_ENABLE_OPENSSL
                  ,
                  SSL* ssl
#endif
        );

    /**
     * @brief 析构函数，清理连接资源。
     * @note 确保连接已断开并释放 socket 和 SSL 资源（如果启用）。
     */
    ~TcpConnection();

    /**
     * @brief 获取所属的事件循环。
     * @return 事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_eventLoop;
    }

    /**
     * @brief 获取连接名称。
     * @return 连接名称。
     */
    const std::string& getName() const {
        return m_name;
    }

    /**
     * @brief 获取本地地址。
     * @return 本地地址智能指针。
     */
    const Address::Ptr& getLocalAddress() const {
        return m_localAddress;
    }

    /**
     * @brief 获取远程地址。
     * @return 远程地址智能指针。
     */
    const Address::Ptr& getPeerAddress() const {
        return m_peerAddress;
    }

    /**
     * @brief 获取用户自定义的上下文（只读）。
     * @return 上下文对象。
     */
    const std::any& getContext() const {
        return m_context;
    }

    /**
     * @brief 获取用户自定义的上下文（可写）。
     * @return 上下文对象的指针。
     */
    std::any* getMutableContext() {
        return &m_context;
    }

    /**
     * @brief 设置用户自定义的上下文。
     * @param[in] context 上下文对象。
     */
    void setContext(const std::any& context) {
        m_context = context;
    }

    /**
     * @brief 设置连接建立/断开的回调函数。
     * @param[in] callback 连接回调函数。
     */
    void setConnectionCallback(ConnectionCallback callback) {
        m_connectionCallback = std::move(callback);
    }

    /**
     * @brief 设置接收消息的回调函数。
     * @param[in] callback 消息回调函数。
     */
    void setMessageCallback(MessageCallback callback) {
        m_messageCallback = std::move(callback);
    }

    /**
     * @brief 设置写完成时的回调函数。
     * @param[in] callback 写完成回调函数。
     */
    void setWriteCompleteCallback(WriteCompleteCallback callback) {
        m_writeCompleteCallback = std::move(callback);
    }

    /**
     * @brief 设置关闭连接时的回调函数。
     * @param[in] callback 关闭回调函数。
     */
    void setCloseCallback(CloseCallback callback) {
        m_closeCallback = std::move(callback);
    }

    /**
     * @brief 设置高水位标记回调函数和阈值。
     * @param[in] callback 高水位标记回调函数。
     * @param[in] highWaterMark 发送缓冲区的高水位阈值（字节）。
     */
    void setHighWaterMarkCallback(HighWaterMarkCallback callback, const size_t highWaterMark) {
        m_highWaterMarkCallback = std::move(callback);
        m_highWaterMark         = highWaterMark;
    }

    /**
     * @brief 检查连接是否已建立。
     * @retval true 连接已建立（状态为 CONNECTED）。
     * @retval false 连接未建立（状态非 CONNECTED）。
     */
    bool isConnected() const {
        return m_state == State::CONNECTED;
    }

    /**
     * @brief 检查连接是否已断开。
     * @retval true 连接已断开（状态为 DISCONNECTED）。
     * @retval false 连接未断开（状态非 DISCONNECTED）。
     */
    bool isDisconnected() const {
        return m_state == State::DISCONNECTED;
    }

    /**
     * @brief 获取 TCP 连接的信息。
     * @param[out] info 存储 TCP 连接信息的结构体。
     * @retval true 成功获取 TCP 连接信息。
     * @retval false 获取 TCP 连接信息失败。
     */
    bool getTcpInfo(tcp_info* info) const;

    /**
     * @brief 发送字符串消息。
     * @param[in] message 待发送的字符串消息。
     * @note 如果连接未建立，忽略发送操作。
     */
    void send(const std::string& message);

    /**
     * @brief 发送缓冲区中的数据（重载版本）。
     * @param[in] buffer 待发送数据的缓冲区。
     * @note 如果连接未建立，忽略发送操作。
     */
    void send(Buffer& buffer);

    /**
     * @brief 优雅关闭连接（关闭写端）。
     * @note 如果连接已断开或正在断开，忽略操作。
     */
    void shutdown();

    /**
     * @brief 强制关闭连接。
     * @note 如果连接已断开，忽略操作。
     */
    void forceClose();

    /**
     * @brief 设置 TCP_NODELAY 属性（禁用/启用 Nagle 算法）。
     * @param[in] on 如果为 true，启用 TCP_NODELAY；否则禁用。
     */
    void setTcpNoDelay(bool on) const;

    /**
     * @brief 开始监听读事件。
     * @note 如果已在读取，忽略操作。
     */
    void startRead();

    /**
     * @brief 停止监听读事件。
     * @note 如果未在读取，忽略操作。
     */
    void stopRead();

    /**
     * @brief 检查是否正在listening读事件。
     * @retval true 正在监听读事件。
     * @retval false 未监听读事件。
     */
    bool isReading() const {
        return m_reading;
    }

    /**
     * @brief 标记连接已建立并开始事件处理。
     * @note 必须在事件循环线程中调用，触发连接回调。
     */
    void connectEstablished();

    /**
     * @brief 标记连接已断开并清理资源。
     * @note 必须在事件循环线程中调用，触发连接和关闭回调。
     */
    void connectDestroyed();

private:
    /**
     * @enum State
     * @brief 表示 TCP 连接的状态。
     */
    enum class State {
        DISCONNECTED,  ///< 连接已断开
        DISCONNECTING, ///< 连接正在断开
        CONNECTING,    ///< 连接正在建立
        CONNECTED      ///< 连接已建立
    };

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @enum SSLState
     * @brief 表示 SSL 连接的状态（仅当启用 ZMUDUO_ENABLE_OPENSSL 时有效）。
     */
    enum class SSLState {
        NONE,        ///< 无 SSL
        HANDSHAKING, ///< SSL 握手进行中
        CONNECTED,   ///< SSL 连接已建立
        FAILED       ///< SSL 连接失败
    };

    /**
     * @brief 继续执行 SSL 握手流程。
     * @note 在读写事件触发时调用，处理 SSL 握手状态。
     */
    void continueSSLHandshake();
#endif

    /**
     * @brief 处理读事件，接收数据到输入缓冲区。
     * @param[in] receiveTime 接收数据的时间戳。
     */
    void handleRead(const Timestamp& receiveTime);

    /**
     * @brief 处理写事件，发送输出缓冲区中的数据。
     */
    void handleWrite();

    /**
     * @brief 处理连接关闭事件。
     */
    void handleClose();

    /**
     * @brief 处理 socket 错误事件。
     */
    void handleError();

    /**
     * @brief 在事件循环线程中发送数据。
     * @param[in] message 待发送的数据指针。
     * @param[in] length 数据长度。
     */
    void sendInLoop(const void* message, size_t length);

    /**
     * @brief 在事件循环线程中强制关闭连接。
     */
    void forceCloseInLoop();

    /**
     * @brief 在事件循环线程中优雅关闭连接（关闭写端）。
     */
    void shutdownInLoop() const;

    /**
     * @brief 在事件循环线程中开始监听读事件。
     */
    void startReadInLoop();

    /**
     * @brief 在事件循环线程中停止监听读事件。
     */
    void stopReadInLoop();

private:
    EventLoop*                 m_eventLoop;                        ///< 所属的事件循环（通常为 subLoop）
    std::string                m_name;                             ///< 连接名称
    std::unique_ptr<TcpSocket> m_socket;                           ///< 连接的 socket
    std::unique_ptr<Channel>   m_channel;                          ///< 事件处理的 Channel
    const Address::Ptr         m_localAddress;                     ///< 本地地址
    const Address::Ptr         m_peerAddress;                      ///< 远程地址
    bool                       m_reading = true;                   ///< 是否正在监听读事件
    State                      m_state   = State::CONNECTING;      ///< 当前连接状态
    ConnectionCallback         m_connectionCallback;               ///< 连接建立/断开回调
    MessageCallback            m_messageCallback;                  ///< 消息接收回调
    WriteCompleteCallback      m_writeCompleteCallback;            ///< 写完成回调
    HighWaterMarkCallback      m_highWaterMarkCallback;            ///< 高水位标记回调
    CloseCallback              m_closeCallback;                    ///< 关闭连接回调
    size_t                     m_highWaterMark = 64 * 1024 * 1024; ///< 发送缓冲区高水位阈值
    Buffer                     m_inputBuffer;                      ///< 输入缓冲区
    Buffer                     m_outputBuffer;                     ///< 输出缓冲区
    std::any                   m_context;                          ///< 用户自定义上下文
#ifdef ZMUDUO_ENABLE_OPENSSL
    SSL*     m_ssl;      ///< SSL 上下文
    SSLState m_sslState; ///< SSL 连接状态
#endif
};
} // namespace zmuduo::net

#endif