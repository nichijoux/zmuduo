// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_WS_WS_SERVER_H
#define ZMUDUO_NET_HTTP_WS_WS_SERVER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/http/http_core.h"
#include "zmuduo/net/http/ws/ws_dispatcher.h"
#include "zmuduo/net/http/ws/ws_frame_parser.h"
#include "zmuduo/net/tcp_server.h"

namespace zmuduo::net::http::ws {
/**
 * @class WSServer
 * @brief WebSocket 服务器类，用于管理 TCP 到 WebSocket 的协议升级和通信。
 *
 * WSServer 封装了 TCP 连接管理、HTTP 握手（RFC 6455）、WebSocket 帧解析和消息分发。<br/>
 * 通过 ServletDispatcher 将消息分发到注册的 Servlet，支持子协议选择和控制帧处理。
 *
 * @note WSServer 不可拷贝（继承自 NoCopyable），通过 EventLoop 确保线程安全。
 * @note Servlet 和回调函数需线程安全，当前仅支持空子协议，未来可扩展支持 JSON、Protobuf 等。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Address::Ptr addr = Address::Create("0.0.0.0", 8000);
 * WSServer server(&loop, addr, "MyWSServer");
 * server.getServletDispatcher().addExactServlet("/echo", [](const WSFrameMessage& msg,
 * TcpConnectionPtr conn) { conn->send(WSFrameMessage(WSFrameHead::TEXT_FRAME,
 * msg.m_payload).serialize(false));
 * });
 * server.start();
 * loop.loop();
 * @endcode
 */
class WSServer : NoCopyable {
  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环。
     * @param[in] listenAddress 监听地址和端口。
     * @param[in] name 服务器名称（用于标识和日志记录）。
     */
    WSServer(EventLoop* loop, const Address::Ptr& listenAddress, const std::string& name);

    /**
     * @brief 默认析构函数
     */
    ~WSServer() = default;

    /**
     * @brief 启动服务器。
     * @note 开始监听连接，初始化 TCP 服务器并注册回调。
     */
    void start();

    /**
     * @brief 设置工作线程数量。
     * @param[in] num 线程数。
     * @note 必须在 start 前调用，影响 TcpServer 的线程池。
     */
    void setThreadNum(int num) {
        m_server.setThreadNum(num);
    }

    /**
     * @brief 获取事件循环对象
     * @return 当前绑定的事件循环
     */
    EventLoop* getEventLoop() const {
        return m_server.getEventLoop();
    }

    /**
     * @brief 获取 Servlet 分发器，用于注册处理不同路径的 WebSocket 请求
     * @return 引用类型的 ServletDispatcher 实例
     */
    ServletDispatcher& getServletDispatcher() {
        return m_dispatcher;
    }

    /**
     * @brief 获取当前支持的子协议
     * @return 当前服务器支持的子协议列表
     */
    std::vector<WSSubProtocol::Ptr>& getSubProtocols() {
        return m_subProtocols;
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    bool loadCertificates(const std::string& certificatePath, const std::string& privateKeyPath) {
        return m_server.loadCertificates(certificatePath, privateKeyPath);
    }

    SSL_CTX* getSSLContext() const {
        return m_server.getSSLContext();
    }
#endif

  private:
    /**
     * @brief 选择支持的子协议。
     * @param[in] subProtocols 客户端请求的子协议列表。
     * @return 选择的子协议实例，若无匹配则返回 nullptr。
     */
    WSSubProtocol::Ptr selectSubProtocol(const std::vector<std::string>& subProtocols);

    /**
     * @brief 处理 TCP 连接建立或断开。
     * @param[in] connection TCP 连接，表示 WebSocket 会话。
     * @note 建立连接时初始化状态和解析器，断开时清理连接信息。
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 处理接收到的数据。
     * @param[in] connection TCP 连接，表示 WebSocket 会话。
     * @param[in,out] buffer 接收缓冲区。
     * @param[in] receiveTime 接收时间戳。
     * @note 在 TCP 状态解析 HTTP 握手请求，在 WEBSOCKET 状态解析帧数据。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

    /**
     * @brief 执行 WebSocket 握手。
     * @param[in] connection TCP 连接。
     * @param[in] request 解析的 HTTP 请求。
     * @note 验证请求头，生成 HTTP 101 响应，切换状态为 WEBSOCKET。
     */
    void httpHandShake(const TcpConnectionPtr& connection, const HttpRequest& request);

    /**
     * @brief 处理 WebSocket 数据通信。
     * @param[in] connection TCP 连接。
     * @param[in,out] buffer 接收缓冲区。
     * @note 解析帧数据，分发到 Servlet 或处理控制帧。
     */
    void onWSCommunication(const TcpConnectionPtr& connection, Buffer& buffer);

  private:
    /**
     * @brief 连接的状态标识枚举
     */
    enum class State {
        TCP       = 1,  ///< 尚未完成 WebSocket 握手
        WEBSOCKET = 2   ///< 已完成 WebSocket 协议升级
    };
    TcpServer         m_server;      ///< 底层 TCP 服务器
    ServletDispatcher m_dispatcher;  ///< Servlet 分发器
    std::unordered_map<TcpConnection*, std::tuple<State, std::string, WSFrameParser::Ptr>>
        m_connections;  ///< 连接状态表（状态、路径、解析器）
    std::vector<WSSubProtocol::Ptr> m_subProtocols;  ///< 支持的子协议列表
};
}  // namespace zmuduo::net::http::ws

#endif