// Copyright (c) nichijoux (BSD 3-Clause)
#ifndef ZMUDUO_NET_HTTP_WS_WS_CLIENT_H
#define ZMUDUO_NET_HTTP_WS_WS_CLIENT_H

#include <utility>

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/http/http_parser.h"
#include "zmuduo/net/http/ws/ws_frame.h"
#include "zmuduo/net/http/ws/ws_frame_parser.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/uri.h"
#include <set>

namespace zmuduo::net::http::ws {
/**
 * @class WSClient
 * @brief WebSocket 客户端类，用于建立和管理 WebSocket 连接。
 *
 * WSClient 提供通过 URI 或服务器地址发起 WebSocket 连接的能力，支持 TCP 连接、HTTP
 * 握手、帧解析和消息通信。<br/> 通过状态机管理连接流程（NONE -> TCP -> HTTP ->
 * WEBSOCKET），支持用户注册连接和消息回调。
 *
 * @note WSClient 不可拷贝（继承自 NoCopyable），通过 EventLoop 确保线程安全。
 * @note 当前仅支持空子协议（Sec-WebSocket-Protocol 为空），未来可扩展支持 JSON、Protobuf 等。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * WSClient client(&loop, "ws://127.0.0.1:8000/chat", "MyClient");
 * client.setWSConnectionCallback([](bool success) {
 *     std::cout << "Connection " << (success ? "established" : "failed") << std::endl;
 * });
 * client.setWSMessageCallback([](const TcpConnectionPtr& conn, const WSFrameMessage& msg) {
 *     std::cout << "Received: " << msg.m_payload << std::endl;
 * });
 * client.connect();
 * loop.loop();
 * @endcode
 */
class WSClient : NoCopyable {
public:
    /// @brief 智能指针
    using Ptr = std::shared_ptr<WSClient>;
    /// @brief 收到websocket信息的回调
    using WSMessageCallback = std::function<void(const TcpConnectionPtr&, const WSFrameMessage&)>;
    /// @brief ws连接建立和断开的回调
    using WSConnectionCallback = std::function<void(bool)>;

public:
    /**
     * @brief 构造函数，通过 URI 字符串创建客户端。
     * @param[in,out] loop 所属事件循环
     * @param[in] uri WebSocket URI 字符串，例如 ws://127.0.0.1:8000/chat
     * @param[in] name 客户端名称（用于标识连接）
     *
     * @throws std::invalid_argument 若 URI 无效或地址创建失败
     */
    WSClient(EventLoop* loop, const std::string& uri, std::string name);

    /**
     * @brief 构造函数，通过 Uri 类创建客户端。
     * @param[in,out] loop 所属事件循环
     * @param[in] uri URI 实例
     * @param[in] name 客户端名称
     *
     * @throws std::invalid_argument 若 uri.createAddress() 返回 nullptr
     */
    WSClient(EventLoop* loop, const Uri& uri, std::string name);

    /**
     * @brief 构造函数，通过服务器地址直接创建连接。
     *
     * @param[in,out] loop 所属事件循环
     * @param[in] serverAddress WebSocket 服务器地址
     * @param[in] name 客户端名称
     * @note 默认的请求路径为/，可通过setPath()函数修改
     */
    WSClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name);

    ~WSClient() = default;

    /**
     * @brief 设置 WebSocket 请求路径（默认路径为 "/"）。
     * @param[in] path 请求的 URI 路径部分，例如 "/chat"
     */
    void setPath(const std::string& path) {
        m_path = path;
    }

    /**
     * @brief 启动连接流程，包括 TCP 连接和 WebSocket 握手。
     */
    void connect();

    /**
     * @brief 停止客户端并释放资源（关闭连接）。
     */
    void stop();

    /**
     * @brief 主动断开连接。
     */
    void disconnect();

    /**
     * @brief 判断当前是否已完成 WebSocket 握手，进入连接状态。
     * @return 是否已连接（状态为 State::WEBSOCKET）
     */
    bool isConnected() {
        return m_state == State::WEBSOCKET;
    }

    /**
     * @brief 发送一帧 WebSocket 数据。
     * @param[in] opcode WebSocket 操作码，如 WSFrameHead::TEXT, WSFrameHead::BINARY
     * @param[in] data 发送内容
     */
    void sendWSFrameMessage(uint8_t opcode, const std::string& data) {
        WSFrameMessage message(opcode, data);
        return sendWSFrameMessage(message);
    }

    /**
     * @brief 发送完整的 WebSocket 数据帧。
     * @param message 已构造好的 WebSocket 帧
     */
    void sendWSFrameMessage(const WSFrameMessage& message);

    /**
     * @brief 设置连接建立/断开的回调函数。
     * @param[in] callback 布尔值为 true 表示连接建立，false 表示断开
     */
    void setWSConnectionCallback(WSConnectionCallback callback) {
        m_connectionCallback = std::move(callback);
    }

    /**
     * @brief 设置收到 WebSocket 应用层消息的回调。
     * @param[in] callback 回调函数（不处理 PING/PONG 等控制帧）
     * @note 控制帧如 PING 会由 WSFramePingPong 自动处理
     */
    void setWSMessageCallback(WSMessageCallback callback) {
        m_messageCallback = std::move(callback);
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 创建并配置 SSL 上下文。
     *
     * 初始化 SSL 上下文，加载系统默认 CA
     * 证书路径，设置对等验证回调，并禁用不安全的协议（SSLv2/SSLv3）。<br/>
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
     * @brief 添加客户端支持的子协议
     * @param[in] subProtocol websocket子协议的一个实现
     */
    void addSupportSubProtocol(const WSSubProtocol::Ptr& subProtocol) {
        if (subProtocol) {
            m_supportProtocols.insert(subProtocol);
        }
    }

private:
    /**
     * @brief 发起 WebSocket 握手请求。
     */
    void doHandShake();

    /**
     * @brief 出现连接或协议错误时的处理逻辑。
     */
    void doWhenError();

    /**
     * @brief 当前连接的状态机枚举。
     */
    enum class State {
        NONE,     ///< 初始状态，未建立任何连接
        TCP,      ///< 已建立 TCP 连接，未进行 HTTP 握手
        HTTP,     ///< 正在进行 HTTP 握手
        WEBSOCKET ///< 握手成功，进入 WebSocket 通信阶段
    };

    /**
     * @brief TCP 连接建立或关闭的回调处理。
     * @param[in] connection 当前的 TCP 连接（表示 WebSocket 会话）
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief TCP 接收数据时的回调处理。
     * @param[in] connection 当前的 TCP 连接（表示 WebSocket 会话）
     * @param[in,out] buffer 接收缓冲区
     * @param[in] receiveTime 接收时间戳
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

private:
    State                                              m_state = State::NONE; ///< 当前连接状态
    TcpClient                                          m_client;              ///< tcp客户端
    std::string                                        m_key;                 ///< 用于tcp通信的key
    std::string                                        m_path = "/";          ///< 请求地址
    std::set<WSSubProtocol::Ptr, WSSubProtocolCompare> m_supportProtocols;    ///< 所支持使用的子协议
    WSSubProtocol::Ptr                                 m_useProtocol;         ///< 当前选择的子协议
    WSConnectionCallback                               m_connectionCallback;  ///< ws连接的回调
    WSMessageCallback                                  m_messageCallback;     ///< ws收到消息的回调
    WSFrameParser                                      m_parser;              ///< websocket数据帧的解析器
};
} // namespace zmuduo::net::http::ws

#endif