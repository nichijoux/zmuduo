#include "zmuduo/net/http/http_dispatcher.h"
#include <fnmatch.h>

namespace zmuduo::net::http {

ServletDispatcher::ServletDispatcher()
    : m_default(std::make_shared<NotFoundServlet>("NotFoundServlet")) {}

void ServletDispatcher::handle(HttpRequest& request, HttpResponse& response) {
    // 1. 执行路径匹配的拦截器
    for (const auto& interceptor : m_interceptors) {
        if (!interceptor->intercept(request, response)) {
            return;  // 被拦截，终止处理
        }
    }
    // 2. 执行过滤器 beforeHandle（可修改 request）
    for (const auto& filter : m_filters) {
        filter->beforeHandle(request);
    }
    // 3. 分发请求给 Servlet
    auto servlet = getMatchedServlet(request.getPath(), request.getMethod());
    if (servlet) {
        servlet->handle(request, response);
    }
    // 4. 执行过滤器 afterHandle（可修改 response）
    for (const auto& filter : m_filters) {
        filter->afterHandle(response);
    }
}

const Servlet::Ptr& ServletDispatcher::getMatchedServlet(const std::string& path,
                                                         HttpMethod         method) const {
    // 1. 首先尝试精确匹配
    // 先查找指定了方法的
    ServletKey keyWithMethod(path, method);
    auto       exactIt = m_exactServlets.find(keyWithMethod);
    if (exactIt != m_exactServlets.end()) {
        return exactIt->second;
    }

    // 再查找不指定方法的
    ServletKey keyWithoutMethod(path);
    exactIt = m_exactServlets.find(keyWithoutMethod);
    if (exactIt != m_exactServlets.end()) {
        return exactIt->second;
    }

    // 2. 尝试通配符匹配
    for (const auto& [key, servlet] : m_wildcardServlets) {
        // 如果key指定了method，则必须匹配；如果没指定，则忽略method检查
        bool methodMatch = !key.method().has_value() || (*key.method() == method);
        if (methodMatch && fnmatch(key.url().c_str(), path.c_str(), 0) == 0) {
            return servlet;
        }
    }

    return m_default;
}

void ServletDispatcher::addExactServlet(ServletKey key, Servlet::Ptr servlet) {
    m_exactServlets.emplace(std::move(key), std::move(servlet));
}

void ServletDispatcher::addExactServlet(ServletKey key, FunctionServlet::ServletCallback callback) {
    m_exactServlets.emplace(std::move(key), std::make_shared<FunctionServlet>(std::move(callback)));
}

void ServletDispatcher::addWildcardServlet(ServletKey key, Servlet::Ptr servlet) {
    m_wildcardServlets.emplace_back(std::move(key), std::move(servlet));
}

void ServletDispatcher::addWildcardServlet(ServletKey                       key,
                                           FunctionServlet::ServletCallback callback) {
    m_wildcardServlets.emplace_back(std::move(key),
                                    std::make_shared<FunctionServlet>(std::move(callback)));
}

bool ServletDispatcher::deleteExactServlet(const ServletKey& key) {
    m_exactServlets.erase(key);
    return true;
}

bool ServletDispatcher::deleteWildcardServlet(const ServletKey& key) {
    auto it = std::find_if(m_wildcardServlets.begin(), m_wildcardServlets.end(),
                           [&](const auto& pair) { return pair.first == key; });
    if (it != m_wildcardServlets.end()) {
        m_wildcardServlets.erase(it);
        return true;
    }
    return false;
}

void ServletDispatcher::addInterceptor(HttpInterceptor::Ptr interceptor) {
    auto it = std::find_if(m_interceptors.begin(), m_interceptors.end(),
                           [&interceptor](const HttpInterceptor::Ptr& existingInterceptor) {
                               return existingInterceptor->getId() == interceptor->getId();
                           });

    if (it != m_interceptors.end()) {
        // 如果找到具有相同 id 的拦截器，替换它
        *it = std::move(interceptor);
    } else {
        // 如果没有找到，将新的拦截器添加到列表中
        m_interceptors.emplace_back(std::move(interceptor));
    }
}

bool ServletDispatcher::deleteInterceptor(const HttpInterceptor::Ptr& interceptor) {
    auto it = std::find_if(m_interceptors.begin(), m_interceptors.end(),
                           [&interceptor](const HttpInterceptor::Ptr& existingInterceptor) {
                               return existingInterceptor->getId() == interceptor->getId();
                           });
    if (it != m_interceptors.end()) {
        m_interceptors.erase(it);
        return true;
    }
    return false;
}

bool ServletDispatcher::deleteInterceptor(const std::string& id) {
    auto it = std::find_if(
        m_interceptors.begin(), m_interceptors.end(),
        [&id](const HttpInterceptor::Ptr& interceptor) { return interceptor->getId() == id; });
    if (it != m_interceptors.end()) {
        m_interceptors.erase(it);
        return true;
    }
    return false;
}

void ServletDispatcher::addFilter(HttpFilter::Ptr filter) {
    auto it = std::find_if(m_filters.begin(), m_filters.end(),
                           [&filter](const HttpFilter::Ptr& existingFilter) {
                               return existingFilter->getId() == filter->getId();
                           });

    if (it != m_filters.end()) {
        // 如果找到具有相同 id 的拦截器，替换它
        *it = std::move(filter);
    } else {
        // 如果没有找到，将新的拦截器添加到列表中
        m_filters.emplace_back(std::move(filter));
    }
}

bool ServletDispatcher::deleteFilter(const HttpFilter::Ptr& filter) {
    auto it = std::find_if(m_filters.begin(), m_filters.end(),
                           [&filter](const HttpFilter::Ptr& existingFilter) {
                               return existingFilter->getId() == filter->getId();
                           });
    if (it != m_filters.end()) {
        m_filters.erase(it);
        return true;
    }
    return false;
}

bool ServletDispatcher::deleteFilter(const std::string& id) {
    auto it = std::find_if(m_filters.begin(), m_filters.end(),
                           [&id](const HttpFilter::Ptr& filter) { return filter->getId() == id; });
    if (it != m_filters.end()) {
        m_filters.erase(it);
        return true;
    }
    return false;
}

}  // namespace zmuduo::net::http