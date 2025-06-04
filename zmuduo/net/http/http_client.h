// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_CLIENT_H
#define ZMUDUO_NET_HTTP_HTTP_CLIENT_H

#include "zmuduo/base/mutex.h"
#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/http/http_context.h"
#include "zmuduo/net/http/http_core.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/uri.h"
#include <queue>

namespace zmuduo::net::http {
/**
 * @class HttpClient
 * @brief HTTP 客户端核心类。
 *
 * HttpClient 基于 TcpClient 实现异步 HTTP/1.1 客户端（符合 RFC 7230-7235），支持
 * GET、POST、PUT、DELETE 等请求。<br/> 通过 HttpResponseCallback 处理响应，支持 HTTPS（需启用
 * ZMUDUO_ENABLE_OPENSSL），并维护请求队列以确保顺序处理。<br/> 适用于 Web 客户端场景，如 API
 * 调用或网页数据获取。
 *
 * @note HttpClient 不可拷贝（继承自 NoCopyable），通过 EventLoop 管理异步 I/O。
 * @note 调用者需确保回调函数的线程安全，所有操作在 EventLoop 线程中执行。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * HttpClient client(&loop, "http://example.com", "MyHttpClient");
 * client.connect();
 * client.doGet("/api", [](HttpResponse::Ptr response) {
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
     * @typedef std::function&lt;void(HttpResponse::Ptr)&gt;
     * @brief HTTP 响应回调函数类型。
     * @param[in] response HTTP 响应对象，失败时为 nullptr。
     */
    using HttpResponseCallback = std::function<void(HttpResponse::Ptr)>;

    /**
     * @typedef std::function&lt;void()&gt;
     * @brief 成功连接的回调
     */
    using ConnectedCallback = std::function<void()>;

  public:
    /**
     * @brief 构造函数（基于 URI 字符串）。
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] uri 目标 URI（如 "http://example.com/api"）。
     * @param[in] name 客户端名称，用于日志记录。
     * @note 从 URI 解析主机和路径，初始化 TcpClient。
     */
    HttpClient(EventLoop* loop, const std::string& uri, std::string name);

    /**
     * @brief 构造函数（基于 URI 对象）。
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] uri 目标 URI 对象。
     * @param[in] name 客户端名称，用于日志记录。
     * @note 支持 http 和 https 协议，从 URI 解析主机和路径。
     */
    HttpClient(EventLoop* loop, const Uri& uri, std::string name);

    /**
     * @brief 构造函数（基于服务器地址）。
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] serverAddress 服务器地址（如 "example.com:80"）。
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
     * @brief 发起 TCP 连接。
     * @note 异步连接到服务器，连接状态通过 onConnection 回调处理。
     */
    void connect() {
        m_client.connect();
    }

    /**
     * @brief 断开 TCP 连接。
     * @note 关闭当前连接，未处理的请求将失败。
     */
    void disconnect() {
        m_client.disconnect();
    }

    /**
     * @brief 停止客户端。
     * @note 停止所有操作，清空请求队列和回调。
     */
    void stop() {
        m_client.stop();
    }

    /**
     * @brief 设置连接成功的回调
     * @param callback 回调函数
     */
    void setConnectedCallback(ConnectedCallback callback) {
        m_connectCallback = std::move(callback);
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 加载 SSL 证书。
     * @param[in] certificatePath 客户端证书路径。
     * @param[in] privateKeyPath 客户端私钥路径。
     * @param[in] caFile CA 证书文件路径（可选）。
     * @param[in] caPath CA 证书目录路径（可选）。
     * @retval true 证书加载成功，支持 HTTPS 连接。
     * @retval false 证书加载失败，无法使用 HTTPS。
     * @note 需在 connect() 前调用，仅在 ZMUDUO_ENABLE_OPENSSL 启用时有效。
     */
    bool loadCertificates(const std::string& certificatePath,
                          const std::string& privateKeyPath,
                          const std::string& caFile = "",
                          const std::string& caPath = "") {
        return m_client.loadCertificates(certificatePath, privateKeyPath, caFile, caPath);
    }
#endif

    /**
     * @brief 发起 GET 请求。
     * @param[in] path 请求路径（如 "/api"）。
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）。
     * @param[in] body 请求体（可选，默认空）。
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doGet(const std::string&   path,
               HttpResponseCallback callback,
               const HeaderMap&     headers = {},
               const std::string&   body    = "");

    /**
     * @brief 发起 POST 请求。
     * @param[in] path 请求路径（如 "/api"）。
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）。
     * @param[in] body 请求体（可选，默认空）。
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doPost(const std::string&   path,
                HttpResponseCallback callback,
                const HeaderMap&     headers = {},
                const std::string&   body    = "");

    /**
     * @brief 发起 PUT 请求。
     * @param[in] path 请求路径（如 "/api"）。
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）。
     * @param[in] body 请求体（可选，默认空）。
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doPut(const std::string&   path,
               HttpResponseCallback callback,
               const HeaderMap&     headers = {},
               const std::string&   body    = "");

    /**
     * @brief 发起 DELETE 请求。
     * @param[in] path 请求路径（如 "/api"）。
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）。
     * @param[in] body 请求体（可选，默认空）。
     * @note 异步发送请求，响应通过 callback 处理。
     */
    void doDelete(const std::string&   path,
                  HttpResponseCallback callback,
                  const HeaderMap&     headers = {},
                  const std::string&   body    = "");

    /**
     * @brief 发起自定义 HTTP 请求。
     * @param[in] method HTTP 方法（如 GET, POST）。
     * @param[in] path 请求路径（如 "/api"）。
     * @param[in] callback 响应回调函数。
     * @param[in] headers 请求头（可选）。
     * @param[in] body 请求体（可选，默认空）。
     * @note 异步发送请求，自动设置 Host 和 Connection 头。
     */
    void doRequest(HttpMethod           method,
                   const std::string&   path,
                   HttpResponseCallback callback,
                   const HeaderMap&     headers = {},
                   const std::string&   body    = "");

    /**
     * @brief 发起自定义 HTTP 请求（基于 HttpRequest 对象）。
     * @param[in] request HTTP 请求对象。
     * @param[in] callback 响应回调函数。
     * @note 异步发送请求，适合复杂请求的构造。
     */
    void doRequest(const HttpRequest& request, HttpResponseCallback callback);

  private:
    /**
     * @brief 连接状态回调。
     * @param[in] connection TCP 连接对象。
     * @note 处理连接建立（发送队列中的请求）和断开（清理未处理请求）。
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
    TcpClient                        m_client;           ///< 底层 TCP 客户端
    std::string                      m_host;             ///< 主机地址（IP:Port 或域名）
    std::string                      m_path;             ///< 默认请求路径（从 URI 解析）
    ConnectedCallback                m_connectCallback;  ///< 成功连接服务器的回调
    std::queue<HttpResponseCallback> m_callbacks;        ///< 响应回调队列
};
}  // namespace zmuduo::net::http

#endif