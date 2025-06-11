// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TCP_CLIENT_H
#define ZMUDUO_NET_TCP_CLIENT_H
#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/callbacks.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/tcp_connection.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#ifdef ZMUDUO_ENABLE_OPENSSL
#    include <openssl/ssl.h>
#endif

namespace zmuduo::net {
class Connector;
using ConnectorPtr = std::shared_ptr<Connector>;

/**
 * @class TcpClient
 * @brief TCP 客户端类，管理与服务器的连接、事件处理和回调。
 *
 * TcpClient 是 Reactor 模型中的核心组件。<br/>
 * 负责发起与服务器的连接（通过 Connector）、管理 TcpConnection
 * 实例、处理重试机制以及消息收发事件。<br/> 支持 SSL 加密（需启用 ZMUDUO_ENABLE_OPENSSL）。
 *
 * @note TcpClient 运行在单一事件循环中，所有方法必须在事件循环线程中调用，以确保线程安全。
 * @note 使用 std::mutex 保护 TcpConnection 实例的访问。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Address::Ptr addr = Address::Create("127.0.0.1:8080");
 * TcpClient client(&loop, addr, "TestClient");
 * client.setConnectionCallback([](const TcpConnectionPtr& conn) {
 *     if (conn->isConnected()) {
 *         conn->send("Hello, server!");
 *     }
 * });
 * client.enableRetry();
 * client.connect();
 * loop.loop();
 * @endcode
 */
class TcpClient : NoCopyable {
  public:
    /**
     * @brief 构造函数，初始化 TCP 客户端。
     * @param[in] loop 事件循环指针。
     * @param[in] serverAddress 远程服务器地址。
     * @param[in] name 客户端名称。
     */
    TcpClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name);

    /**
     * @brief 析构函数，清理客户端资源。
     * @note 断开连接并释放 SSL 上下文（如果启用）。
     */
    ~TcpClient();

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 创建并配置 SSL 上下文。
     *
     * 初始化 SSL 上下文，加载系统默认 CA 证书路径，设置对等验证回调，并禁用不安全的协议（SSLv2/SSLv3）。<br/>
     * 若客户端已连接或上下文已存在，则失败。
     *
     * @retval true SSL 上下文创建和配置成功。
     * @retval false 创建失败（已连接、上下文已存在或 OpenSSL 错误）。
     *
     * @note 必须在建立连接前调用，失败时记录错误日志并释放上下文。
     * @note 验证回调记录证书错误详情，供调试使用。
     * @note 使用 SSLv23_client_method 兼容 TLS 协议，禁用 SSLv2/SSLv3 和压缩。
     */
    bool createSSLContext();

    /**
     * @brief 加载自定义客户端证书和私钥。
     *
     * 加载指定路径的客户端证书和私钥（PEM 格式），用于双向认证，并验证私钥与证书匹配。<br/>
     * 若客户端已连接或未创建 SSL 上下文，则失败。
     *
     * @param[in] certificatePath 客户端证书文件路径（PEM 格式）。
     * @param[in] privateKeyPath 私钥文件路径（PEM 格式）。
     * @retval true 证书和私钥加载成功且匹配。
     * @retval false 加载失败（已连接、无上下文、文件无效或不匹配）。
     *
     * @note 必须在 createSSLContext 后、连接前调用。
     * @note 证书和私钥文件需为 PEM 格式，路径需可访问。
     * @note 失败时释放 SSL 上下文并记录错误日志。
     */
    bool loadCustomCertificate(const std::string& certificatePath,
                               const std::string& privateKeyPath);

    /**
     * @brief 加载自定义 CA 证书或路径。
     *
     * 加载指定的 CA 证书文件或目录，用于验证服务器证书。若未提供 CA 文件或路径，则跳过加载。
     * 若客户端已连接或未创建 SSL 上下文，则失败。
     *
     * @param[in] caFile CA 证书文件路径（PEM 格式，可为空）。
     * @param[in] caPath CA 证书目录路径（可为空）。
     * @retval true CA 证书加载成功或未提供参数。
     * @retval false 加载失败（已连接、无上下文或 OpenSSL 错误）。
     *
     * @note 必须在 createSSLContext 后、连接前调用。
     * @note CA 文件/目录需为 PEM 格式，路径需可访问。
     * @note 失败时记录 OpenSSL 错误日志。
     */
    bool loadCustomCACertificate(const std::string& caFile, const std::string& caPath);

    /**
     * @brief 设置 SSL 主机名。
     *
     * 设置用于服务器名称指示（SNI）的目标主机名，在建立 SSL 连接时使用。
     *
     * @param[in] hostname 目标主机名。
     * @note 主机名为空字符串，不会调用SSL_set_tlsext_host_name函数。
     */
    void setSSLHostName(std::string hostname) {
        m_sslHostname = std::move(hostname);
    }

    /**
     * @brief 获取 SSL 上下文指针。
     *
     * 返回当前配置的 SSL 上下文，供外部检查或管理。
     *
     * @return SSL_CTX* SSL 上下文指针，可能为空。
     * @note 调用者需自行检查指针有效性，不负责上下文的释放。
     */
    SSL_CTX* getSSLContext() const {
        return m_sslContext;
    }
#endif

    /**
     * @brief 发起与服务器的连接。
     * @note 如果已连接，忽略操作；异步启动 Connector。
     */
    void connect();

    /**
     * @brief 停止客户端。
     * @note 停止 Connector 并标记未连接状态。
     */
    void stop();

    /**
     * @brief 断开当前连接。
     * @note 如果存在连接，优雅关闭；线程安全。
     */
    void disconnect();

    /**
     * @brief 获取事件循环。
     * @return 事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_eventLoop;
    }

    /**
     * @brief 获取客户端名称。
     * @return 客户端名称。
     */
    const std::string& getName() const {
        return m_name;
    }

    /**
     * @brief 查询客户端是否连接服务器成功
     * @return 是否连接成功
     */
    bool isConnected() const {
        return m_connected;
    }

    /**
     * @brief 发送字符串消息。
     * @param[in] message 待发送的字符串消息。
     * @note 如果连接未建立，忽略发送操作。
     */
    void send(const std::string& message) {
        if (m_connected) {
            m_connection->send(message);
        }
    }

    /**
     * @brief 发送缓冲区中的数据（重载版本）。
     * @param[in] buffer 待发送数据的缓冲区。
     * @note 如果连接未建立，忽略发送操作。
     */
    void send(Buffer& buffer) {
        if (m_connected) {
            m_connection->send(buffer);
        }
    }

    /**
     * @brief 获取当前连接。
     * @return TcpConnection 智能指针，可能为空。
     */
    TcpConnectionPtr getConnection() const {
        return m_connection;
    }

    /**
     * @brief 检查是否启用重试机制。
     * @retval true 已启用重试机制。
     * @retval false 未启用重试机制。
     */
    bool isRetry() const {
        return m_retry;
    }

    /**
     * @brief 启用重试机制。
     * @note 连接断开后自动尝试重新连接。
     */
    void enableRetry() {
        m_retry = true;
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

  private:
    /**
     * @brief 处理新连接。
     * @param[in] socketFD 新连接的 socket 文件描述符。
     * @note 创建 TcpConnection 实例并设置回调。
     */
    void newConnection(int socketFD);

    /**
     * @brief 移除指定的连接。
     * @param[in] connection 要移除的 TcpConnection 智能指针。
     * @note 清理连接并尝试重连（如果启用重试）。
     */
    void removeConnection(const TcpConnectionPtr& connection);

  private:
    static std::atomic<int64_t> S_NEXT_ID;                ///< 用于生成连接名称的计数器
    EventLoop*                  m_eventLoop;              ///< 事件循环
    ConnectorPtr                m_connector;              ///< 连接器
    const std::string           m_name;                   ///< 客户端名称
    bool                        m_retry;                  ///< 是否启用重试机制
    bool                        m_connected;              ///< 是否已连接
    ConnectionCallback          m_connectionCallback;     ///< 连接建立/断开回调
    MessageCallback             m_messageCallback;        ///< 消息接收回调
    WriteCompleteCallback       m_writeCompleteCallback;  ///< 写完成回调
    std::mutex                  m_mutex;                  ///< 保护 m_connection 的互斥锁
    TcpConnectionPtr            m_connection;             ///< 当前连接
#ifdef ZMUDUO_ENABLE_OPENSSL
    SSL_CTX*    m_sslContext;   ///< SSL 上下文
    std::string m_sslHostname;  ///< SSL服务器主机名,用于SNI和证书验证
#endif
};
}  // namespace zmuduo::net

#endif