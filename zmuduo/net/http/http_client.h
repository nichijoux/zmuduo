// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_CLIENT_H
#define ZMUDUO_NET_HTTP_HTTP_CLIENT_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/http/http_context.h"
#include "zmuduo/net/http/http_core.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/uri.h"
#include <optional>
#include <queue>

namespace zmuduo::net::http {
/**
 * @class HttpClient
 * @brief HTTP 客户端核心类。
 *
 * HttpClient 基于 TcpClient 实现异步 HTTP/1.1 客户端，支持GET、POST、PUT、DELETE 等请求。<br/>
 * 通过 HttpResponseCallback
 * 处理响应，支持HTTPS（需启用ZMUDUO_ENABLE_OPENSSL），并维护请求队列以确保顺序处理。<br/> 适用于
 * Web 客户端场景，如 API 调用或网页数据获取。
 *
 * @note HttpClient 不可拷贝（继承自 NoCopyable），通过 EventLoop 管理异步 I/O。
 * @note 调用者需确保回调函数的线程安全，所有操作在 EventLoop 线程中执行。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * HttpClient client(&loop, "http://example.com", "MyHttpClient");
 * client.doGet("/api", [](const auto& response) {
 *     if (response) {
 *         std::cout << response->toString() << std::endl;
 *     } else {
 *         std::cerr << "Request failed" << std::endl;
 *     }
 * });
 * loop.loop();
 * @endcode
 */
class HttpClient : NoCopyable {
public:
    /**
     * @typedef std::map&lt;std::string, std::string&gt;
     * @brief HTTP 请求头映射类型。
     */
    using HeaderMap = std::map<std::string, std::string>;

    /**
     * @typedef std::function&lt;void(std::optional&lt;HttpResponse&gt;)&gt;
     * @brief HTTP 响应回调函数类型。
     * @param[in] response HTTP 响应对象，失败时为 std::nullopt。
     */
    using HttpResponseCallback = std::function<void(std::optional<HttpResponse>)>;

public:
    /**
     * @brief 构造函数（基于 URI 字符串）
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] uri 目标 URI（如 "http://example.com/api"）
     * @param[in] name 客户端名称，用于日志记录。
     * @note 从 URI 解析主机和路径，初始化 TcpClient。
     */
    HttpClient(EventLoop* loop, const std::string& uri, std::string name);

    /**
     * @brief 构造函数（基于 URI 对象）
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] uri 目标 URI 对象。
     * @param[in] name 客户端名称，用于日志记录。
     * @note 支持 http 和 https 协议，从 URI 解析主机和路径。
     */
    HttpClient(EventLoop* loop, const Uri& uri, std::string name);

    /**
     * @brief 构造函数（基于服务器地址）
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] serverAddress 服务器地址（如 "example.com:80"）
     * @param[in] name 客户端名称，用于日志记录。
     * @note 直接使用服务器地址，路径需在请求时指定。
     */
    HttpClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name);

    /**
     * @brief 析构函数。
     * @note 确保 TcpClient 正确销毁，释放资源。
     */
    ~HttpClient() = default;

    /**
     * @brief 设置是否重新连接
     * @param reconnect
     */
    void setReConnect(const bool reconnect) {
        m_reconnect = reconnect;
    }

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
     * @brief 发起 GET 请求。
     * @param[in] path 请求路径（如 "/api"）
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）
     * @param[in] body 请求体（可选，默认空）
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doGet(const std::string&   path,
               HttpResponseCallback callback,
               const HeaderMap&     headers = {},
               const std::string&   body    = "",
               uint32_t             timeout = 30);

    /**
     * @brief 发起 POST 请求。
     * @param[in] path 请求路径（如 "/api"）
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）
     * @param[in] body 请求体（可选，默认空）
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doPost(const std::string&   path,
                HttpResponseCallback callback,
                const HeaderMap&     headers = {},
                const std::string&   body    = "",
                uint32_t             timeout = 30);

    /**
     * @brief 发起 PUT 请求。
     * @param[in] path 请求路径（如 "/api"）
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）
     * @param[in] body 请求体（可选，默认空）
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doPut(const std::string&   path,
               HttpResponseCallback callback,
               const HeaderMap&     headers = {},
               const std::string&   body    = "",
               uint32_t             timeout = 30);

    /**
     * @brief 发起 DELETE 请求。
     * @param[in] path 请求路径（如 "/api"）
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）
     * @param[in] body 请求体（可选，默认空）
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doDelete(const std::string&   path,
                  HttpResponseCallback callback,
                  const HeaderMap&     headers = {},
                  const std::string&   body    = "",
                  uint32_t             timeout = 30);

    /**
     * @brief 发起自定义 HTTP 请求。
     * @param[in] method HTTP 方法（如 GET, POST）
     * @param[in] path 请求路径（如 "/api"）
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）
     * @param[in] body 请求体（可选，默认空）
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，自动设置 Host 和 Connection 头。
     */
    void doRequest(HttpMethod           method,
                   const std::string&   path,
                   HttpResponseCallback callback,
                   const HeaderMap&     headers = {},
                   const std::string&   body    = "",
                   uint32_t             timeout = 30);

    /**
     * @brief 发起自定义 HTTP 请求（基于 HttpRequest 对象）
     * @param[in] request HTTP 请求对象。
     * @param[in] callback 响应回调函数。
     * @param[in] timeout 超时时间，单位秒（可选，默认30s）
     * @note 异步发送请求，适合复杂请求的构造。
     */
    void
    doRequest(const HttpRequest& request, HttpResponseCallback callback, uint32_t timeout = 30);

private:
    /**
     * @brief 连接状态回调。
     * @param[in] connection TCP 连接对象。
     * @note 处理连接建立（发送队列中的请求）和断开（清理未处理请求）
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 消息接收回调。
     * @param[in] connection TCP 连接对象。
     * @param[in,out] buffer 接收缓冲区。
     * @param[in] receiveTime 数据接收时间。
     * @note 解析 HTTP 响应，调用回调处理结果，处理错误或连接关闭。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

private:
    TcpClient                m_client;    ///< 底层 TCP 客户端
    std::string              m_host;      ///< 主机地址（IP:Port 或域名）
    std::string              m_path;      ///< 默认请求路径（从 URI 解析）
    bool                     m_reconnect; ///< 如果还有没有发送完的请求,断开连接后是否需要重新连接（默认为true）
    std::unique_ptr<TimerId> m_timerId;   ///< 当前发送请求对应的超时定时器
    std::queue<std::tuple<HttpRequest, HttpResponseCallback, uint32_t>>
    m_callbacks; ///< 请求、响应回调、超时时间队列
};
} // namespace zmuduo::net::http

#endif