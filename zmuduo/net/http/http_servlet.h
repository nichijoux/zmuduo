// Copyright (c) nichijoux[2025] (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_SERVLET_H
#define ZMUDUO_NET_HTTP_HTTP_SERVLET_H

#include "zmuduo/net/callbacks.h"
#include "zmuduo/net/http/http_core.h"
#include <functional>
#include <string>
#include <utility>

namespace zmuduo::net::http {
/**
 * @class Servlet
 * @brief HTTP 请求处理基类。
 *
 * Servlet 提供 HTTP 请求处理的抽象接口，用于 Web 服务器处理客户端请求。<br/>
 * 派生类需实现 handle 方法以定义具体的请求处理逻辑。支持拷贝（继承自 Copyable），通过智能指针管理。
 *
 * @note Servlet 通常在 Web 服务器（如 HttpServer）中注册，用于处理特定路径或模式的请求。
 *
 * @example
 *
 * @code
 * class CustomServlet : public Servlet {
 * public:
 *     CustomServlet() : Servlet("CustomServlet") {}
 *     void handle(const HttpRequest& request, HttpResponse& response) override {
 *         response.setStatus(HttpStatus::OK);
 *         response.setBody("Hello from CustomServlet!");
 *     }
 * };
 * @endcode
 */
class Servlet : public Copyable {
public:
    /**
     * @typedef std::shared_ptr&lt;Servlet&gt;
     * @brief Servlet 智能指针类型。
     */
    using Ptr = std::shared_ptr<Servlet>;

public:
    /**
     * @brief 构造函数。
     * @param[in] name Servlet 名称，用于标识或日志记录。
     */
    explicit Servlet(std::string name)
        : m_name(std::move(name)) {}

    /**
     * @brief 虚析构函数，确保派生类正确析构。
     */
    virtual ~Servlet() = default;

    /**
     * @brief 处理 HTTP 请求。
     * @param[in] request 输入的 HTTP 请求对象。
     * @param[out] response 输出的 HTTP 响应对象，需由实现填充状态、头部和消息体。
     * @note 派生类需实现此方法，定义具体的请求处理逻辑。
     */
    virtual void handle(const HttpRequest& request, HttpResponse& response) = 0;

    /**
     * @brief 获取 Servlet 名称。
     * @return Servlet 名称字符串。
     */
    const std::string& getName() const {
        return m_name;
    }

protected:
    std::string m_name; ///< Servlet 名称
};

/**
 * @class FunctionServlet
 * @brief 基于回调函数的 Servlet。
 *
 * FunctionServlet 允许通过 std::function 回调定义请求处理逻辑，提供灵活的处理方式。<br/>
 * 继承自 Servlet，默认名称为 "FunctionServlet"。
 *
 * @note 适合动态注册处理逻辑的场景，例如临时路由或测试。
 *
 * @example
 *
 * @code
 * FunctionServlet::ServletCallback cb = [](const HttpRequest& request, HttpResponse& response) {
 *     response.setStatus(HttpStatus::OK);
 *     response.setBody("Handled by FunctionServlet!");
 * };
 * FunctionServlet servlet(cb);
 * HttpRequest request;
 * HttpResponse response;
 * servlet.handle(request, response);
 * @endcode
 */
class FunctionServlet final : public Servlet {
public:
    /**
     * @typedef std::function&lt;void(const HttpRequest& request, HttpResponse& response)&gt;
     * @brief 回调函数类型，定义请求处理逻辑。
     * @param[in] request 输入的 HTTP 请求对象。
     * @param[out] response 输出的 HTTP 响应对象。
     */
    using ServletCallback = std::function<void(const HttpRequest& request, HttpResponse& response)>;

public:
    /**
     * @brief 构造函数。
     * @param[in] callback 处理请求的回调函数。
     */
    explicit FunctionServlet(ServletCallback callback)
        : Servlet("FunctionServlet"),
          m_callback(std::move(callback)) {}

    /**
     * @brief 处理 HTTP 请求。
     * @param[in] request 输入的 HTTP 请求对象。
     * @param[out] response 输出的 HTTP 响应对象。
     * @note 调用构造函数中提供的回调函数执行处理逻辑。
     */
    void handle(const HttpRequest& request, HttpResponse& response) override {
        m_callback(request, response);
    }

private:
    ServletCallback m_callback; ///< 请求处理回调函数
};

/**
 * @class NotFoundServlet
 * @brief 默认 404 Not Found 响应 Servlet。
 *
 * NotFoundServlet 用于处理未找到资源的请求，返回标准的 404 HTML 页面。<br/>
 * 继承自 Servlet，生成包含服务器名称的 404 响应。
 *
 * @note 通常作为 Web 服务器的默认 Servlet，处理无法匹配的请求。
 *
 * @example
 *
 * @code
 * NotFoundServlet servlet("zmuduo/1.0.0");
 * HttpRequest request;
 * HttpResponse response;
 * servlet.handle(request, response);
 * std::cout &lt;&lt; response.toString() &gt;&gt; std::endl; // 输出 404 响应
 * @endcode
 */
class NotFoundServlet final : public Servlet {
public:
    /**
     * @brief 构造函数。
     * @param[in] name Servlet 名称，通常为服务器标识（如 "zmuduo/1.0.0"）。
     * @note 初始化 404 HTML 内容，包含指定的名称。
     */
    explicit NotFoundServlet(std::string name);

    /**
     * @brief 处理 HTTP 请求，返回 404 响应。
     * @param[in] request 输入的 HTTP 请求对象。
     * @param[out] response 输出的 HTTP 响应对象，设置为 404 状态并包含 HTML 内容。
     */
    void handle(const HttpRequest& request, HttpResponse& response) override;

private:
    std::string m_content; ///< 404 响应的 HTML 内容
};
} // namespace zmuduo::net::http

#endif