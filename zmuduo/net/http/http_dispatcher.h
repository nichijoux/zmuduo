// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_SERVLET_DISPATCHER_H
#define ZMUDUO_NET_HTTP_HTTP_SERVLET_DISPATCHER_H

#include "zmuduo/net/http/http_core.h"
#include "zmuduo/net/http/http_filter.h"
#include "zmuduo/net/http/http_interceptor.h"
#include "zmuduo/net/http/http_servlet.h"
#include <optional>

namespace zmuduo::net::http {

/**
 * @class ServletKey
 * @brief Servlet查找键，包含URL路径和可选的HTTP方法
 *
 * 用于唯一标识一个Servlet，支持精确匹配和通配符匹配。
 * 当method为std::nullopt时，表示匹配所有HTTP方法。
 */
class ServletKey {
  public:
    /**
     * @brief 构造函数
     * @param url URL路径
     * @param method HTTP方法
     */
    ServletKey(const char* uri, HttpMethod method) : m_url(uri), m_method(method) {}

    /**
     * @brief 构造函数
     * @param url URL路径
     * @param method 可选的HTTP方法（默认为std::nullopt，匹配所有方法）
     */
    explicit ServletKey(std::string url, std::optional<HttpMethod> method = std::nullopt)
        : m_url(std::move(url)), m_method(method) {}

    /**
     * @brief 从C字符串隐式构造
     * @param url URL路径（自动转换为std::string）
     */
    explicit ServletKey(const char* url) : m_url(url), m_method(std::nullopt) {}

    /// 获取URL路径
    const std::string& url() const {
        return m_url;
    }

    /// 获取HTTP方法（可能为std::nullopt）
    const std::optional<HttpMethod>& method() const {
        return m_method;
    }

    /// 相等比较运算符
    bool operator==(const ServletKey& other) const {
        return m_url == other.m_url && m_method == other.m_method;
    }

    /**
     * @struct Hash
     * @brief ServletKey的哈希函数，用于unordered_map
     */
    struct Hash {
        size_t operator()(const ServletKey& key) const {
            return std::hash<std::string>()(key.m_url) ^
                   std::hash<bool>()(key.m_method.has_value()) ^
                   (key.m_method.has_value() ? std::hash<int>()(static_cast<int>(*key.m_method)) :
                                               0);
        }
    };

  private:
    std::string               m_url;     ///< 请求地址
    std::optional<HttpMethod> m_method;  ///< 可访问的方法
};

/**
 * @class ServletDispatcher
 * @brief HTTP Servlet 分发器，负责将请求分发到匹配的 Servlet。
 *
 * 根据 HTTP 请求的 URL 路径和方法分发到对应的 Servlet，支持精确匹配（如
 * "/user/login"）、通配符匹配（如 "/api/*"）和默认处理（NotFoundServlet）。
 * 集成拦截器（HttpInterceptor）和过滤器（HttpFilter）链，分别用于请求拦截和请求/响应修改。
 *
 * @note 匹配优先级：精确匹配 > 通配符匹配（按注册顺序） > 默认 Servlet。
 * @note 方法匹配优先级：指定方法的 Servlet > 不指定方法的 Servlet。
 * @note ServletDispatcher 可拷贝（继承自 Copyable），线程安全，但回调函数需用户确保线程安全。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/http/http_dispatcher.h"
 * #include <iostream>
 *
 * int main() {
 *     ServletDispatcher dispatcher;
 *     // 添加精确匹配 Servlet
 *     dispatcher.addExactServlet("/api/user", [](HttpRequest& req, HttpResponse& resp) {
 *         resp.setStatus(HttpResponse::OK);
 *         resp.setBody("User API");
 *     }, HttpMethod::GET);
 *     // 添加通配符匹配 Servlet
 *     dispatcher.addWildcardServlet("/public/*", [](HttpRequest& req, HttpResponse& resp) {
 *         resp.setStatus(HttpResponse::OK);
 *         resp.setBody("Public resource");
 *     });
 *     // 添加拦截器
 *     dispatcher.addInterceptor("AuthInterceptor", [](const HttpRequest& req, HttpResponse& resp) {
 *         if (req.getHeader("Authorization").empty()) {
 *             resp.setStatus(HttpResponse::UNAUTHORIZED);
 *             return false;
 *         }
 *         return true;
 *     });
 *     // 添加过滤器
 *     dispatcher.addFilter("LogFilter",
 *         [](HttpRequest& req) { std::cout << "Request: " << req.getPath() << std::endl; },
 *         [](HttpResponse& resp) { std::cout << "Response: " << resp.getStatus() << std::endl; }
 *     );
 *     HttpRequest req; req.setPath("/api/user"); req.setMethod(HttpMethod::GET);
 *     HttpResponse resp;
 *     dispatcher.handle(req, resp);
 *     return 0;
 * }
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
     * @brief 处理 HTTP 请求。
     * @param[in,out] request HTTP 请求对象。
     * @param[out] response HTTP 响应对象。
     * @note 执行流程：拦截器 -> 过滤器 beforeHandle -> Servlet 处理 -> 过滤器 afterHandle。
     * @note 若拦截器返回 false，处理终止，直接返回 response。
     */
    void handle(HttpRequest& request, HttpResponse& response);

    /**
     * @brief 添加精确匹配 Servlet。
     * @param[in] key 包含 URL 和可选方法的 ServletKey。
     * @param[in] servlet Servlet 实例。
     */
    void addExactServlet(ServletKey key, Servlet::Ptr servlet);

    /**
     * @brief 添加精确匹配 Servlet（回调形式）。
     * @param[in] key 包含 URL 和可选方法的 ServletKey。
     * @param[in] callback Servlet 处理回调函数。
     */
    void addExactServlet(ServletKey key, FunctionServlet::ServletCallback callback);

    /**
     * @brief 添加精确匹配 Servlet（便捷方法）。
     * @param[in] uri URL 路径。
     * @param[in] servlet Servlet 实例。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，匹配所有方法）。
     */
    void addExactServlet(const std::string&        uri,
                         Servlet::Ptr              servlet,
                         std::optional<HttpMethod> method = std::nullopt) {
        addExactServlet(ServletKey(uri, method), std::move(servlet));
    }

    /**
     * @brief 添加精确匹配 Servlet（回调形式，便捷方法）。
     * @param[in] uri URL 路径。
     * @param[in] callback Servlet 处理回调函数。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，匹配所有方法）。
     */
    void addExactServlet(const std::string&               uri,
                         FunctionServlet::ServletCallback callback,
                         std::optional<HttpMethod>        method = std::nullopt) {
        addExactServlet(ServletKey(uri, method), std::move(callback));
    }

    /**
     * @brief 添加通配符匹配 Servlet。
     * @param[in] key 包含 URL 和可选方法的 ServletKey。
     * @param[in] servlet Servlet 实例。
     */
    void addWildcardServlet(ServletKey key, Servlet::Ptr servlet);

    /**
     * @brief 添加通配符匹配 Servlet（回调形式）。
     * @param[in] key 包含 URL 和可选方法的 ServletKey。
     * @param[in] callback Servlet 处理回调函数。
     */
    void addWildcardServlet(ServletKey key, FunctionServlet::ServletCallback callback);

    /**
     * @brief 添加通配符匹配 Servlet（便捷方法）。
     * @param[in] uri 通配符 URL 路径（如 "/api/*"）。
     * @param[in] servlet Servlet 实例。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，匹配所有方法）。
     */
    void addWildcardServlet(const std::string&        uri,
                            Servlet::Ptr              servlet,
                            std::optional<HttpMethod> method = std::nullopt) {
        addWildcardServlet(ServletKey(uri, method), std::move(servlet));
    }

    /**
     * @brief 添加通配符匹配 Servlet（回调形式，便捷方法）。
     * @param[in] uri 通配符 URL 路径（如 "/api/*"）。
     * @param[in] callback Servlet 处理回调函数。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，匹配所有方法）。
     */
    void addWildcardServlet(const std::string&               uri,
                            FunctionServlet::ServletCallback callback,
                            std::optional<HttpMethod>        method = std::nullopt) {
        addWildcardServlet(ServletKey(uri, method), std::move(callback));
    }

    /**
     * @brief 删除精确匹配 Servlet。
     * @param[in] key 要删除的 ServletKey。
     * @retval true 删除成功。
     * @retval false 未找到匹配的 Servlet。
     */
    bool deleteExactServlet(const ServletKey& key);

    /**
     * @brief 删除精确匹配 Servlet（便捷方法）。
     * @param[in] uri URL 路径。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，删除所有方法的匹配项）。
     * @retval true 删除成功。
     * @retval false 未找到匹配的 Servlet。
     */
    bool deleteExactServlet(const std::string&        uri,
                            std::optional<HttpMethod> method = std::nullopt) {
        return deleteExactServlet(ServletKey(uri, method));
    }

    /**
     * @brief 删除通配符匹配 Servlet。
     * @param[in] key 要删除的 ServletKey。
     * @retval true 删除成功。
     * @retval false 未找到匹配的 Servlet。
     */
    bool deleteWildcardServlet(const ServletKey& key);

    /**
     * @brief 删除通配符匹配 Servlet（便捷方法）。
     * @param[in] uri 通配符 URL 路径。
     * @param[in] method 可选的 HTTP 方法（默认为 std::nullopt，删除所有方法的匹配项）。
     * @retval true 删除成功。
     * @retval false 未找到匹配的 Servlet。
     */
    bool deleteWildcardServlet(const std::string&        uri,
                               std::optional<HttpMethod> method = std::nullopt) {
        return deleteWildcardServlet(ServletKey(uri, method));
    }

    /**
     * @brief 添加拦截器。
     * @param[in] interceptor 拦截器实例。
     * @note 若存在相同 ID 的拦截器，将替换原有实例。
     */
    void addInterceptor(HttpInterceptor::Ptr interceptor);

    /**
     * @brief 添加拦截器（回调形式）。
     * @param[in] id 拦截器唯一标识符。
     * @param[in] callback 拦截器回调函数。
     */
    void addInterceptor(std::string id, FunctionInterceptor::InterceptorCallback callback) {
        addInterceptor(std::make_shared<FunctionInterceptor>(std::move(id), std::move(callback)));
    }

    /**
     * @brief 删除拦截器。
     * @param[in] interceptor 拦截器实例。
     * @retval true 删除成功。
     * @retval false 未找到匹配的拦截器。
     */
    bool deleteInterceptor(const HttpInterceptor::Ptr& interceptor);

    /**
     * @brief 删除拦截器（通过 ID）。
     * @param[in] id 拦截器唯一标识符。
     * @retval true 删除成功。
     * @retval false 未找到匹配的拦截器。
     */
    bool deleteInterceptor(const std::string& id);

    /**
     * @brief 添加过滤器。
     * @param[in] filter 过滤器实例。
     * @note 若存在相同 ID 的过滤器，将替换原有实例。
     */
    void addFilter(HttpFilter::Ptr filter);

    /**
     * @brief 添加过滤器（回调形式）。
     * @param[in] id 过滤器唯一标识符。
     * @param[in] beforeCallback 请求过滤回调。
     * @param[in] afterCallback 响应过滤回调。
     */
    void addFilter(std::string                    id,
                   FunctionFilter::BeforeCallback beforeCallback,
                   FunctionFilter::AfterCallback  afterCallback) {
        addFilter(std::make_shared<FunctionFilter>(std::move(id), std::move(beforeCallback),
                                                   std::move(afterCallback)));
    }

    /**
     * @brief 删除过滤器。
     * @param[in] filter 过滤器实例。
     * @retval true 删除成功。
     * @retval false 未找到匹配的过滤器。
     */
    bool deleteFilter(const HttpFilter::Ptr& filter);

    /**
     * @brief 删除过滤器（通过 ID）。
     * @param[in] id 过滤器唯一标识符。
     * @retval true 删除成功。
     * @retval false 未找到匹配的过滤器。
     */
    bool deleteFilter(const std::string& id);

    /**
     * @brief 设置默认 Servlet。
     * @param[in] servlet 默认 Servlet 实例。
     */
    void setDefaultServlet(Servlet::Ptr servlet) {
        m_default = std::move(servlet);
    }

    /**
     * @brief 设置默认 Servlet（回调形式）。
     * @param[in] callback 默认 Servlet 处理回调函数。
     */
    void setDefaultServlet(FunctionServlet::ServletCallback callback) {
        m_default = std::make_shared<FunctionServlet>(std::move(callback));
    }

  private:
    /**
     * @brief 查找匹配的 Servlet。
     * @param[in] path 请求路径。
     * @param[in] method 请求方法。
     * @return 匹配的 Servlet 智能指针引用。
     * @note 优先级：精确匹配（指定方法 > 不指定方法） > 通配符匹配（按注册顺序） > 默认 Servlet。
     */
    const Servlet::Ptr& getMatchedServlet(const std::string& path, HttpMethod method) const;

  private:
    std::unordered_map<ServletKey, Servlet::Ptr, ServletKey::Hash> m_exactServlets;  ///< 精确匹配表
    std::vector<std::pair<ServletKey, Servlet::Ptr>> m_wildcardServlets;  ///< 通配符匹配表
    Servlet::Ptr                                     m_default;           ///< 默认 Servlet
    std::vector<HttpInterceptor::Ptr>                m_interceptors;      ///< 拦截器链
    std::vector<HttpFilter::Ptr>                     m_filters;           ///< 过滤器链
};

}  // namespace zmuduo::net::http

#endif