// Copyright (c) nichijoux[2025] (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_INTERCEPTOR_H
#define ZMUDUO_NET_HTTP_HTTP_INTERCEPTOR_H

#include "zmuduo/net/http/http_core.h"
#include <functional>
#include <memory>
#include <utility>

namespace zmuduo::net::http {
/**
 * @class HttpInterceptor
 * @brief HTTP 拦截器抽象基类，用于拦截和处理 HTTP 请求。
 *
 * HttpInterceptor 提供拦截接口，允许检查 HTTP 请求并决定是否继续传递到后续处理（如
 * Servlet）或直接返回响应。 适用于认证、限流、黑名单检查等场景。
 *
 * @note HttpInterceptor 可拷贝（继承自 Copyable），线程安全，但派生类的实现需确保线程安全。
 * @note 拦截器通常按注册顺序组成链式处理，若返回 false，请求处理将终止，response 直接发送给客户端。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/http/http_interceptor.h"
 * #include <iostream>
 *
 * class AuthInterceptor : public HttpInterceptor {
 * public:
 *     AuthInterceptor() : HttpInterceptor("AuthInterceptor") {}
 *     bool intercept(const HttpRequest& request, HttpResponse& response) override {
 *         if (request.getHeader("Authorization").empty()) {
 *             response.setStatus(HttpResponse::UNAUTHORIZED);
 *             response.setBody("Unauthorized");
 *             return false;
 *         }
 *         return true;
 *     }
 * };
 *
 * // 使用 FunctionInterceptor
 * auto interceptor = std::make_shared<FunctionInterceptor>(
 *     "RateLimitInterceptor",
 *     [](const HttpRequest& req, HttpResponse& resp) {
 *         static int count = 0;
 *         if (++count > 100) {
 *             resp.setStatus(HttpResponse::TOO_MANY_REQUESTS);
 *             resp.setBody("Rate limit exceeded");
 *             return false;
 *         }
 *         return true;
 *     }
 * );
 * @endcode
 */
class HttpInterceptor : public Copyable {
public:
    using Ptr = std::shared_ptr<HttpInterceptor>; ///< 智能指针类型

public:
    /**
     * @brief 构造函数。
     * @param[in] id 拦截器唯一标识符。
     */
    explicit HttpInterceptor(std::string id)
        : m_id(std::move(id)) {}

    /**
     * @brief 虚析构函数。
     * @note 确保派生类正确析构。
     */
    virtual ~HttpInterceptor() = default;

    /**
     * @brief 拦截并处理 HTTP 请求。
     * @param[in] request HTTP 请求，只读。
     * @param[out] response HTTP 响应，若返回 false，此响应将直接发送给客户端。
     * @retval true 请求通过拦截器，继续传递到后续处理。
     * @retval false 请求被拦截，response 将直接发送给客户端。
     * @note 实现可用于认证、限流、黑名单检查等。
     */
    virtual bool intercept(const HttpRequest& request, HttpResponse& response) = 0;

    /**
     * @brief 获取拦截器唯一标识符。
     * @return 拦截器 ID。
     */
    const std::string& getId() const {
        return m_id;
    }

private:
    std::string m_id; ///< 拦截器唯一标识符
};

/**
 * @class FunctionInterceptor
 * @brief 基于回调函数的 HTTP 拦截器实现。
 *
 * FunctionInterceptor 通过用户提供的回调函数实现 intercept，支持灵活的拦截逻辑。
 * 适合快速定义简单的拦截器，无需创建新类。
 *
 * @note 回调函数需线程安全，建议使用 lambda 或线程安全的函数对象。
 *
 * @example
 *
 * @code
 * auto interceptor = std::make_shared<FunctionInterceptor>(
 *     "BlacklistInterceptor",
 *     [](const HttpRequest& req, HttpResponse& resp) {
 *         if (req.getRemoteAddr() == "192.168.1.100") {
 *             resp.setStatus(HttpResponse::FORBIDDEN);
 *             resp.setBody("Access denied");
 *             return false;
 *         }
 *         return true;
 *     }
 * );
 * @endcode
 */
class FunctionInterceptor final : public HttpInterceptor {
public:
    using InterceptorCallback =
    std::function<bool(const HttpRequest&, HttpResponse&)>; ///< 拦截回调

public:
    /**
     * @brief 构造函数。
     * @param[in] id 拦截器唯一标识符。
     * @param[in] cb 拦截回调，执行 intercept 逻辑。
     */
    explicit FunctionInterceptor(std::string id, InterceptorCallback cb)
        : HttpInterceptor(std::move(id)),
          m_callback(std::move(cb)) {}

    /**
     * @brief 拦截并处理 HTTP 请求。
     * @param[in] request HTTP 请求，只读。
     * @param[out] response HTTP 响应，若返回 false，此响应将直接发送给客户端。
     * @retval true 请求通过拦截器，继续传递到后续处理。
     * @retval false 请求被拦截，response 将直接发送给客户端。
     * @note 执行 m_callback，若回调为空则返回 true。
     */
    bool intercept(const HttpRequest& request, HttpResponse& response) override {
        return !m_callback || m_callback(request, response);
    }

private:
    InterceptorCallback m_callback; ///< 拦截回调
};
} // namespace zmuduo::net::http

#endif