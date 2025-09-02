// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_WS_WS_SERVLET_H
#define ZMUDUO_NET_HTTP_WS_WS_SERVLET_H

#include "zmuduo/base/copyable.h"
#include "zmuduo/net/callbacks.h"
#include "zmuduo/net/http/ws/ws_frame.h"
#include <memory>
#include <string>
#include <utility>

namespace zmuduo::net::http::ws {
/**
 * @class Servlet
 * @brief WebSocket 消息处理基类。
 *
 * Servlet 是一个抽象基类，定义 WebSocket 消息处理接口，用于处理接收到的 WSFrameMessage。<br/>
 * 派生类通过实现 handle 方法提供具体的消息处理逻辑，类似于 HTTP 的 Servlet 模型。
 *
 * @note Servlet 可拷贝（继承自 Copyable），通过智能指针（Ptr）管理。
 * @note handle 方法在 EventLoop 线程中执行，派生类需确保线程安全。
 *
 * @example
 *
 * @code
 * class MyServlet : public Servlet {
 * public:
 *     MyServlet() : Servlet("MyServlet") {}
 *     void handle(const WSFrameMessage& message, TcpConnectionPtr connection) override {
 *         std::string response = "Echo: " + message.m_payload;
 *         connection->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, response).serialize(false));
 *     }
 * };
 * @endcode
 */
class Servlet : public Copyable {
public:
    /**
     * @typedef std::shared_ptr&lt;Servlet&gt;
     * @brief Servlet的智能指针
     */
    using Ptr = std::shared_ptr<Servlet>;

public:
    /**
     * @brief 构造函数。
     * @param[in] name Servlet 名称，用于标识和日志记录。
     */
    explicit Servlet(std::string name)
        : m_name(std::move(name)) {}

    /**
     * @brief 析构函数
     */
    virtual ~Servlet() = default;

    /**
     * @brief 处理 WebSocket 消息。
     * @param[in] message 接收到的 WebSocket 帧消息。
     * @param[in,out] connection TCP 连接，表示 WebSocket 会话。
     * @note 纯虚函数，派生类必须实现具体的处理逻辑。
     */
    virtual void handle(const WSFrameMessage& message, TcpConnectionPtr connection) = 0;

protected:
    std::string m_name; ///< Servlet 名称
};

/**
 * @class FunctionServlet
 * @brief 函数式 WebSocket Servlet。
 *
 * FunctionServlet 通过回调函数实现 WebSocket 消息处理，提供灵活的处理逻辑。<br/>
 * 继承自 Servlet，使用 std::function 封装用户提供的回调。
 *
 * @example
 *
 * @code
 * FunctionServlet::ServletCallback cb = [](const WSFrameMessage& msg, TcpConnectionPtr conn) {
 *     conn->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, "Received: " +
 * msg.m_payload).serialize(false));
 * };
 * FunctionServlet servlet(cb);
 * @endcode
 */
class FunctionServlet final : public Servlet {
public:
    /**
     * @typedef std::function&lt;void(const WSFrameMessage& message, TcpConnectionPtr
     * connection)&gt;
     * @brief 函数回调
     * @param[in] message 接收到的 WebSocket 帧消息。
     * @param[in,out] connection TCP 连接，表示 WebSocket 会话。
     */
    using ServletCallback =
    std::function<void(const WSFrameMessage& message, TcpConnectionPtr connection)>;

    /**
     * @brief 构造函数。
     * @param[in] callback 用户提供的消息处理回调函数。
     * @note Servlet 名称固定为 "FunctionServlet"。
     */
    explicit FunctionServlet(ServletCallback callback)
        : Servlet("FunctionServlet"),
          m_callback(std::move(callback)) {}

    /**
     * @brief 处理 WebSocket 消息。
     * @param[in] message 接收到的 WebSocket 帧消息。
     * @param[in,out] connection TCP 连接，表示 WebSocket 会话。
     * @note 调用用户提供的回调函数处理消息。
     */
    void handle(const WSFrameMessage& message, const TcpConnectionPtr connection) override {
        m_callback(message, connection);
    }

private:
    ServletCallback m_callback; ///< 用户回调函数
};

/**
 * @class NotFoundServlet
 * @brief 未找到 WebSocket 请求的默认 Servlet。
 *
 * NotFoundServlet 处理未匹配的 WebSocket 请求，返回状态码 1000（NORMAL_CLOSURE）的 CLOSE 帧。
 *
 * @example
 *
 * @code
 * NotFoundServlet servlet;
 * WSFrameMessage msg(WSFrameHead::TEXT_FRAME, "Test");
 * servlet.handle(msg, connection); // 发送 CLOSE 帧并关闭连接
 * @endcode
 */
class NotFoundServlet final : public Servlet {
public:
    /**
     * @brief 构造函数。
     * @note Servlet 名称固定为 "NotFound"。
     */
    NotFoundServlet();

    /**
     * @brief 处理 WebSocket 消息。
     * @param[in] message 接收到的 WebSocket 帧消息。
     * @param[in,out] connection TCP 连接，表示 WebSocket 会话。
     * @note 发送状态码 1000 的 CLOSE 帧，关闭连接。
     */
    void handle(const WSFrameMessage& message, TcpConnectionPtr connection) override;
};
} // namespace zmuduo::net::http::ws

#endif