// Copyright (c) nichijoux[2025] (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_SERVER_H
#define ZMUDUO_NET_HTTP_SERVER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/http/http_dispatcher.h"
#include "zmuduo/net/http/http_servlet.h"
#include "zmuduo/net/tcp_server.h"

namespace zmuduo::net::http {
/**
 * @class HttpServer
 * @brief HTTP 服务器核心类。
 *
 * HttpServer 基于 TcpServer 实现异步 HTTP/1.1 服务器（符合 RFC 7230-7235），负责监听 TCP 连接、解析
 * HTTP 请求、通过 ServletDispatcher 分发请求，并发送响应。<br/>
 * 支持长连接（Keep-Alive）和多线程处理。<br/>
 * 适用于 Web 服务器场景，如 RESTful API 或网页服务。
 *
 * @note HttpServer 不可拷贝（继承自 NoCopyable），通过 EventLoop 管理异步 I/O。
 * @note 调用者需确保 Servlet 实现的线程安全，所有操作在 EventLoop 线程中执行。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Address::Ptr addr = Address::Create("0.0.0.0", 8080);
 * HttpServer server(&loop, addr, "MyHttpServer", true);
 * server.getServletDispatcher().addServlet("/hello", [](const HttpRequest& req, HttpResponse& res)
 * { res.setStatus(HttpStatus::OK); res.setBody("Hello, World!");
 * });
 * server.setThreadNum(4);
 * server.start();
 * loop.loop();
 * @endcode
 */
class HttpServer : NoCopyable {
  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环，管理异步 I/O。
     * @param[in] listenAddress 监听地址（如 "0.0.0.0:8080"）。
     * @param[in] name 服务器名称，用于日志和响应头部。
     * @param[in] keepAlive 是否启用长连接（默认 false）。
     * @param[in] option TcpServer 配置选项（默认 false）。
     * @note 初始化 TcpServer 和 ServletDispatcher，设置连接和消息回调。
     */
    explicit HttpServer(EventLoop*          loop,
                        const Address::Ptr& listenAddress,
                        const std::string&  name,
                        bool                keepAlive = false,
                        bool                reusePort = false);

    /**
     * @brief 析构函数。
     * @note 确保 TcpServer 正确销毁，释放资源。
     */
    ~HttpServer() = default;

    /**
     * @brief 获取服务器名称。
     * @return 服务器名称。
     */
    const std::string& getName() const {
        return m_server.getName();
    }

    /**
     * @brief 获取事件循环。
     * @return 事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_server.getEventLoop();
    }

    /**
     * @brief 设置工作线程数。
     * @param[in] num 工作线程数（0 表示单线程）。
     * @note 必须在 start() 前调用。
     */
    void setThreadNum(int num) {
        m_server.setThreadNum(num);
    }

    /**
     * @brief 获取 Servlet 分发器。
     * @return Servlet 分发器引用。
     * @note 用于注册或修改 Servlet 路由。
     */
    ServletDispatcher& getServletDispatcher() {
        return m_dispatcher;
    }

    /**
     * @brief 设置 Servlet 分发器。
     * @param[in] v 新的 Servlet 分发器实例。
     */
    void setServletDispatcher(const ServletDispatcher& v) {
        m_dispatcher = v;
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 加载自定义服务器证书和私钥。
     *
     * 加载指定路径的服务器证书和私钥（PEM 格式），并验证私钥与证书匹配。<br/>
     * 若服务器已连接或未创建 SSL 上下文，则失败。
     *
     * @param[in] certificatePath 客户端证书文件路径（PEM 格式）。
     * @param[in] privateKeyPath 私钥文件路径（PEM 格式）。
     * @retval true 证书和私钥加载成功且匹配。
     * @retval false 加载失败（已连接、无上下文、文件无效或不匹配）。
     *
     * @note 必须在连接前调用。
     * @note 证书和私钥文件需为 PEM 格式，路径需可访问。
     * @note 失败时释放 SSL 上下文并记录错误日志。
     */
    bool loadCertificates(const std::string& certificatePath, const std::string& privateKeyPath) {
        return m_server.loadCertificates(certificatePath, privateKeyPath);
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
        return m_server.getSSLContext();
    }
#endif

    /**
     * @brief 启动服务器。
     * @note 开始监听连接，记录启动日志。
     */
    void start();

    /**
     * @brief 设置是否启用长连接。
     * @param[in] keepAlive 是否启用长连接。
     */
    void setKeepAlive(bool keepAlive) {
        m_keepAlive = keepAlive;
    }

    /**
     * @brief 检查是否启用长连接。
     * @retval true 长连接已启用。
     * @retval false 长连接未启用。
     */
    bool isKeepAlive() const {
        return m_keepAlive;
    }

  private:
    /**
     * @brief 连接回调函数。
     * @param[in] connection TCP 连接智能指针。
     * @note 连接建立时初始化 HttpContext，连接断开时记录日志。
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 消息回调函数。
     * @param[in] connection TCP 连接智能指针。
     * @param[in,out] buffer 输入输出缓冲区，包含接收到的数据。
     * @param[in] receiveTime 接收时间戳。
     * @note 解析请求、分发处理、发送响应，并根据 keepAlive 和请求状态决定是否关闭连接。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

  private:
    TcpServer         m_server;      ///< 底层 TCP 服务器
    ServletDispatcher m_dispatcher;  ///< Servlet 分发器
    bool              m_keepAlive;   ///< 是否支持长连接
};
}  // namespace zmuduo::net::http

#endif