// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_EMAIL_SMTP_CLIENT_H
#define ZMUDUO_NET_EMAIL_SMTP_CLIENT_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/email/email.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/uri.h"
#include <deque>

namespace zmuduo::net::email {
/**
 * @class SmtpClient
 * @brief 异步 SMTP 协议客户端，用于发送电子邮件。
 *
 * SmtpClient 基于 TCP 连接实现 SMTP 协议（RFC 5321），通过事件驱动模型异步发送邮件。<br/>
 * 支持多收件人、抄送、密送、MIME 实体（如附件），并提供成功和失败回调。<br/>
 * 使用状态机管理 SMTP 交互流程，支持 SSL/TLS 加密（需启用 OpenSSL）。
 *
 * @note SmtpClient 不可拷贝（继承自 NoCopyable），通过 TcpClient 管理连接。
 * @note 所有操作通过 EventLoop 异步执行，确保线程安全。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * SmtpClient client(&loop, "smtp.example.com:25", "MySMTP");
 * client.setSuccessCallback([] { std::cout << "Mail sent.\n"; });
 * client.setFailureCallback([](const std::string& msg) { std::cerr << msg << "\n"; });
 * auto email = EMail::Create("sender@example.com", "password", "Test", "Hello",
 *                            {"receiver@example.com"});
 * client.send(email);
 * loop.loop();
 * @endcode
 */
class SmtpClient : NoCopyable {
public:
    /**
     * @typedef std::function&lt;void()&gt;
     * @brief 成功发送邮件的回调函数类型。
     */
    using SuccessCallback = std::function<void()>;

    /**
     * @typedef std::function&lt;void(const std::string& message)&gt;
     * @brief 发送失败的回调函数类型。
     * @param message 错误信息。
     */
    using FailureCallback = std::function<void(const std::string& message)>;

public:
    /**
     * @brief 构造函数，使用 URI 字符串创建 SMTP 客户端。
     * @param[in] loop 事件循环。
     * @param[in] uri SMTP 服务器 URI（如 "smtp://smtp.example.com:25"）。
     * @param[in] name 客户端名称。
     * @throw std::invalid_argument 如果 URI 解析失败或 scheme 不是 "smtp"。
     */
    SmtpClient(EventLoop* loop, const std::string& uri, std::string name);

    /**
     * @brief 构造函数，使用 Uri 对象创建 SMTP 客户端。
     * @param[in] loop 事件循环。
     * @param[in] uri 已解析的 Uri 对象。
     * @param[in] name 客户端名称。
     * @throw std::invalid_argument 如果 scheme 不是 "smtp" 或地址解析失败。
     */
    SmtpClient(EventLoop* loop, const Uri& uri, std::string name);

    /**
     * @brief 构造函数，使用主机地址创建 SMTP 客户端。
     * @param[in] loop 事件循环。
     * @param[in] hostAddress SMTP 服务器地址。
     * @param[in] name 客户端名称。
     */
    SmtpClient(EventLoop* loop, const Address::Ptr& hostAddress, std::string name) noexcept;

    /**
     * @brief 析构函数，清理资源。
     * @note 自动断开 TCP 连接并释放资源。
     */
    ~SmtpClient() = default;

    /**
     * @brief 异步发送邮件。
     * @param[in] email 待发送的邮件对象。
     * @note 邮件发送操作在事件循环中异步执行。
     */
    void send(const EMail::Ptr& email);

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
    bool createSSLContext() {
        return m_client.createSSLContext();
    }

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
                               const std::string& privateKeyPath) {
        return m_client.loadCustomCertificate(certificatePath, privateKeyPath);
    }

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
    bool loadCustomCACertificate(const std::string& caFile, const std::string& caPath) {
        return m_client.loadCustomCACertificate(caFile, caPath);
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
        return m_client.getSSLContext();
    }
#endif

    /**
     * @brief 设置邮件发送成功的回调函数。
     * @param[in] callback 成功回调函数。
     */
    void setSuccessCallback(SuccessCallback callback) {
        m_successCallback = std::move(callback);
    }

    /**
     * @brief 设置邮件发送失败的回调函数。
     * @param[in] callback 失败回调函数。
     */
    void setFailureCallback(FailureCallback callback) {
        m_failureCallback = std::move(callback);
    }

private:
    /**
     * @enum State
     * @brief SMTP 协议状态机状态。
     */
    enum class State {
        DISCONNECT,     ///< 未连接
        CONNECTED,      ///< 已连接，待发送 HELO
        HELO_SENT,      ///< 已发送 HELO
        AUTH_SENT,      ///< 已发送 AUTH LOGIN
        USERNAME_SENT,  ///< 已发送用户名
        PASSWORD_SENT,  ///< 已发送密码
        MAIL_FROM_SENT, ///< 已发送 MAIL FROM
        RCPT_TO_SENT,   ///< 已发送 RCPT TO
        DATA_SENT,      ///< 已发送 DATA
        QUIT_SENT,      ///< 已发送 QUIT
        FINISHED        ///< 邮件发送完成
    };

    /**
     * @brief 在事件循环中发送邮件。
     * @param[in] email 待发送的邮件对象。
     * @note 初始化 SMTP 命令序列并启动连接。
     */
    void sendInLoop(const EMail::Ptr& email);

    /**
     * @brief 处理 TCP 连接事件。
     * @param[in] connection TCP 连接对象。
     * @note 更新状态机并处理连接断开。
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 处理 SMTP 服务器响应。
     * @param[in] connection TCP 连接对象。
     * @param[in,out] buffer 接收缓冲区。
     * @param[in] receiveTime 接收时间戳。
     * @note 解析服务器响应并驱动状态机。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

    /**
     * @brief 处理 SMTP 服务器响应。
     * @param[in] response 服务器响应字符串。
     * @note 根据状态机和响应代码执行下一步操作。
     */
    void handleResponse(const std::string& response);

    /**
     * @brief 发送下一条 SMTP 命令。
     * @note 从命令队列中获取并发送命令，或完成发送流程。
     */
    void sendNextCommand();

    /**
     * @brief 发送 SMTP 命令。
     * @param[in] cmd 命令字符串。
     * @note 通过 TCP 连接发送命令并记录日志。
     */
    void sendCommand(const std::string& cmd) const;

    /**
     * @brief 处理错误。
     * @param[in] error 错误信息。
     * @note 触发失败回调并断开连接。
     */
    void handleError(const std::string& error);

private:
    TcpClient               m_client;          ///< TCP 客户端连接
    State                   m_state;           ///< 当前 SMTP 状态
    std::deque<std::string> m_commands;        ///< 待发送的 SMTP 命令队列
    SuccessCallback         m_successCallback; ///< 成功回调函数
    FailureCallback         m_failureCallback; ///< 失败回调函数
};
} // namespace zmuduo::net::email

#endif