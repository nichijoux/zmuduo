// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_WS_WS_DISPATCHER_H
#define ZMUDUO_NET_HTTP_WS_WS_DISPATCHER_H

#include "zmuduo/net/http/ws/ws_servlet.h"

namespace zmuduo::net::http::ws {
/**
 * @class ServletDispatcher
 * @brief WebSocket Servlet 分发器。
 *
 * ServletDispatcher 负责将 WebSocket 请求分发到匹配的 Servlet，支持精确 URI 匹配、通配符 URI 匹配和默认处理。<br/>
 * 匹配优先级为：精确匹配 > 通配符匹配 > 默认 Servlet。默认 Servlet 为 NotFoundServlet，发送状态码 1000 的 CLOSE 帧。
 *
 * @note ServletDispatcher 可拷贝（继承自 Copyable），通过智能指针管理 Servlet。
 * @note 分发操作在 EventLoop 线程中执行，Servlet 实现需确保线程安全。
 *
 * @example
 *
 * @code
 * ServletDispatcher dispatcher;
 * dispatcher.addExactServlet("/echo", [](const WSFrameMessage& msg, TcpConnectionPtr conn) {
 *     conn->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, msg.m_payload).serialize(false));
 * });
 * dispatcher.addWildcardServlet("/chat/*", std::make_shared<MyServlet>());
 * WSFrameMessage msg(WSFrameHead::TEXT_FRAME, "Hello");
 * dispatcher.handle("/echo", msg, connection); // 分发到 /echo 的 Servlet
 * @endcode
 */
class ServletDispatcher : public Copyable {
public:
    /**
     * @brief 构造函数。
     * @note 初始化默认 Servlet 为 NotFoundServlet。
     */
    ServletDispatcher();

    /**
     * @brief 分发 WebSocket 消息。
     * @param[in] uri 请求的 URI。
     * @param[in] message WebSocket 帧消息。
     * @param[in] connection TCP 连接，表示 WebSocket 会话。
     * @note 根据 URI 查找匹配的 Servlet 并调用其 handle 方法。
     */
    void handle(const std::string& uri, const WSFrameMessage& message, TcpConnectionPtr connection) const;

    /**
     * @brief 添加精确匹配的 Servlet。
     * @param[in] uri 精确 URI（如 "/echo"）。
     * @param[in] servlet Servlet 实例。
     * @note 覆盖同名 URI 的现有 Servlet。
     */
    void addExactServlet(const std::string& uri, Servlet::Ptr servlet);

    /**
     * @brief 添加精确匹配的函数式 Servlet。
     * @param[in] uri 精确 URI（如 "/echo"）。
     * @param[in] callback FunctionServlet 的回调函数。
     * @note 创建 FunctionServlet 实例并添加到精确匹配映射。
     */
    void addExactServlet(const std::string& uri, FunctionServlet::ServletCallback callback);

    /**
     * @brief 添加通配符匹配的 Servlet。
     * @param[in] uri 通配符 URI（如 "/chat/*"）。
     * @param[in] servlet Servlet 实例。
     * @note 添加到通配符匹配列表，匹配顺序为添加顺序。
     */
    void addWildcardServlet(const std::string& uri, Servlet::Ptr servlet);

    /**
     * @brief 添加通配符匹配的函数式 Servlet。
     * @param[in] uri 通配符 URI（如 "/chat/*"）。
     * @param[in] callback FunctionServlet 的回调函数。
     * @note 创建 FunctionServlet 实例并添加到通配符匹配列表。
     */
    void addWildcardServlet(const std::string& uri, FunctionServlet::ServletCallback callback);

    /**
     * @brief 删除精确匹配的 Servlet。
     * @param[in] uri 精确 URI。
     * @note 如果 URI 不存在，则无操作。
     */
    void deleteExactServlet(const std::string& uri);

    /**
     * @brief 删除通配符匹配的 Servlet。
     * @param[in] uri 通配符 URI。
     * @note 删除第一个匹配的通配符 Servlet。
     */
    void deleteWildcardServlet(const std::string& uri);

    /**
     * @brief 设置默认 Servlet。
     * @param[in] servlet 默认 Servlet 实例。
     * @note 覆盖现有的默认 Servlet。
     */
    void setDefaultServlet(Servlet::Ptr servlet) {
        m_default = std::move(servlet);
    }

    /**
     * @brief 设置默认函数式 Servlet。
     * @param[in] callback FunctionServlet 的回调函数。
     * @note 创建 FunctionServlet 实例作为默认 Servlet。
     */
    void setDefaultServlet(FunctionServlet::ServletCallback callback) {
        m_default = std::make_shared<FunctionServlet>(std::move(callback));
    }

private:
    /**
     * @brief 获取匹配的 Servlet。
     * @param[in] path 请求的 URI。
     * @return 匹配的 Servlet 智能指针，若无匹配则返回默认 Servlet。
     * @note 优先进行精确匹配，若失败则尝试通配符匹配，最后返回默认 Servlet。
     */
    const Servlet::Ptr& getMatchedServlet(const std::string& path) const;

private:
    std::unordered_map<std::string, Servlet::Ptr> m_exactServlets; ///< 精确 URI 的 Servlet 映射
    std::vector<std::pair<std::string, Servlet::Ptr>>
    m_wildcardServlets;     ///< 通配符 URI 的 Servlet 映射
    Servlet::Ptr m_default; ///< 默认 Servlet
};
} // namespace zmuduo::net::http::ws

#endif