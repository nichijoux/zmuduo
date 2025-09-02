// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_FILTER_H
#define ZMUDUO_NET_HTTP_HTTP_FILTER_H

#include "zmuduo/net/http/http_core.h"
#include <functional>
#include <memory>
#include <utility>

namespace zmuduo::net::http {
/**
 * @class HttpFilter
 * @brief HTTP 过滤器抽象基类，用于在 Servlet 处理前后拦截和修改请求/响应。
 *
 * HttpFilter 提供过滤接口，允许在 HTTP 请求进入 Servlet
 * 前（beforeHandle）和响应返回前（afterHandle）执行自定义逻辑。
 * 适用于日志记录、认证、数据压缩、请求/响应修改等场景。
 *
 * @note HttpFilter 可拷贝（继承自 Copyable），线程安全，但派生类的实现需确保线程安全。
 * @note 过滤器通常按注册顺序组成链式处理，需注意性能开销。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/http/http_filter.h"
 * #include <iostream>
 *
 * class LoggingFilter : public HttpFilter {
 * public:
 *     LoggingFilter() : HttpFilter("LoggingFilter") {}
 *     void beforeHandle(HttpRequest& request) override {
 *         std::cout << "Request: " << request.getMethod() << " " << request.getPath() << std::endl;
 *     }
 *     void afterHandle(HttpResponse& response) override {
 *         std::cout << "Response: " << response.getStatus() << std::endl;
 *     }
 * };
 *
 * // 使用 FunctionFilter
 * auto filter = std::make_shared<FunctionFilter>(
 *     "HeaderFilter",
 *     [](HttpRequest& req) { req.addHeader("X-Custom", "Value"); },
 *     [](HttpResponse& resp) { resp.addHeader("X-Server", "Zmuduo"); }
 * );
 * @endcode
 */
class HttpFilter : public Copyable {
public:
    /**
     * @typedef std::shared_ptr&lt;HttpFilter&gt;
     * @brief HttpFilter的智能指针类型
     */
    using Ptr = std::shared_ptr<HttpFilter>;

public:
    /**
     * @brief 构造函数。
     * @param[in] id 过滤器唯一标识符。
     */
    explicit HttpFilter(std::string id)
        : m_id(std::move(id)) {}

    /**
     * @brief 虚析构函数。
     * @note 确保派生类正确析构。
     */
    virtual ~HttpFilter() = default;

    /**
     * @brief 在请求进入 Servlet 前调用。
     * @param[in,out] request HTTP 请求，可修改（如添加头字段）。
     * @note 实现可用于日志记录、认证、请求预处理等。
     */
    virtual void beforeHandle(HttpRequest& request) = 0;

    /**
     * @brief 在 Servlet 处理完成后调用。
     * @param[in,out] response HTTP 响应，可修改（如添加头字段、压缩内容）。
     * @note 实现可用于响应后处理、日志记录等。
     */
    virtual void afterHandle(HttpResponse& response) = 0;

    /**
     * @brief 获取过滤器标识符。
     * @return 过滤器唯一标识符。
     */
    const std::string& getId() const {
        return m_id;
    }

private:
    std::string m_id; ///< 过滤器唯一标识符
};

/**
 * @class FunctionFilter
 * @brief 基于回调函数的 HTTP 过滤器实现。
 *
 * FunctionFilter 通过用户提供的回调函数实现 beforeHandle 和 afterHandle，支持灵活的过滤逻辑。
 * 适合快速定义简单的过滤器，无需创建新类。
 *
 * @note 回调函数需线程安全，建议使用 lambda 或线程安全的函数对象。
 *
 * @example
 *
 * @code
 * auto filter = std::make_shared<FunctionFilter>(
 *     "AuthFilter",
 *     [](HttpRequest& req) {
 *         if (req.getHeader("Authorization").empty()) {
 *             req.setPath("/unauthorized");
 *         }
 *     },
 *     [](HttpResponse& resp) {
 *         resp.addHeader("X-Filtered", "true");
 *     }
 * );
 * @endcode
 */
class FunctionFilter final : public HttpFilter {
public:
    /**
     * @brief 请求过滤回调
     * @typedef std::function&lt;void(HttpRequest&)&gt;
     * @param[in,out] request http请求
     */
    using BeforeCallback = std::function<void(HttpRequest& request)>;
    /**
     * @brief 响应过滤回调
     * @typedef std::function&lt;void(HttpResponse&)&gt;
     * @param[in,out] response http响应
     */
    using AfterCallback = std::function<void(HttpResponse& response)>;

public:
    /**
     * @brief 构造函数。
     * @param[in] id 过滤器唯一标识符。
     * @param[in] before 请求过滤回调，执行 beforeHandle 逻辑。
     * @param[in] after 响应过滤回调，执行 afterHandle 逻辑。
     */
    FunctionFilter(std::string id, BeforeCallback before, AfterCallback after)
        : HttpFilter(std::move(id)),
          m_beforeCallback(std::move(before)),
          m_afterCallback(std::move(after)) {}

    /**
     * @brief 在请求进入 Servlet 前调用。
     * @param[in,out] request HTTP 请求，可修改。
     * @note 执行 m_beforeCallback，若回调为空则无操作。
     */
    void beforeHandle(HttpRequest& request) override {
        if (m_beforeCallback) {
            m_beforeCallback(request);
        }
    }

    /**
     * @brief 在 Servlet 处理完成后调用。
     * @param[in,out] response HTTP 响应，可修改。
     * @note 执行 m_afterCallback，若回调为空则无操作。
     */
    void afterHandle(HttpResponse& response) override {
        if (m_afterCallback) {
            m_afterCallback(response);
        }
    }

private:
    BeforeCallback m_beforeCallback; ///< 请求过滤回调
    AfterCallback  m_afterCallback;  ///< 响应过滤回调
};
} // namespace zmuduo::net::http

#endif