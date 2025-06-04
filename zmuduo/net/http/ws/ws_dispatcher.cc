#include "zmuduo/net/http/ws/ws_dispatcher.h"
#include "zmuduo/base/logger.h"
#include <fnmatch.h>

#include <utility>

namespace zmuduo::net::http::ws {
ServletDispatcher::ServletDispatcher() : m_default(std::make_shared<NotFoundServlet>()) {}

void ServletDispatcher::handle(const std::string&    uri,
                               const WSFrameMessage& message,
                               TcpConnectionPtr      connection) {
    auto servlet = getMatchedServlet(uri);
    if (servlet) {
        servlet->handle(message, std::move(connection));
    }
}

const Servlet::Ptr& ServletDispatcher::getMatchedServlet(const std::string& path) const {
    // 1.首先尝试精确匹配
    auto exactIt = m_exactServlets.find(path);
    if (exactIt != m_exactServlets.end()) {
        return exactIt->second;
    }
    // 2.尝试通配符匹配
    for (const auto& [k, v] : m_wildcardServlets) {
        if (fnmatch(k.c_str(), path.c_str(), 0) == 0) {
            return v;
        }
    }
    return m_default;
}

void ServletDispatcher::addExactServlet(const std::string& uri, Servlet::Ptr servlet) {
    m_exactServlets[uri] = std::move(servlet);
}

void ServletDispatcher::addExactServlet(const std::string&               uri,
                                        FunctionServlet::ServletCallback callback) {
    m_exactServlets[uri] = std::make_shared<FunctionServlet>(std::move(callback));
}

void ServletDispatcher::addWildcardServlet(const std::string& uri, Servlet::Ptr servlet) {
    m_wildcardServlets.emplace_back(uri, std::move(servlet));
}

void ServletDispatcher::addWildcardServlet(const std::string&               uri,
                                           FunctionServlet::ServletCallback callback) {
    m_wildcardServlets.emplace_back(uri, std::make_shared<FunctionServlet>(std::move(callback)));
}

void ServletDispatcher::deleteExactServlet(const std::string& uri) {
    m_exactServlets.erase(uri);
}

void ServletDispatcher::deleteWildcardServlet(const std::string& uri) {
    auto it = std::find_if(m_wildcardServlets.begin(), m_wildcardServlets.end(),
                           [&](const auto& pair) { return pair.first == uri; });
    if (it != m_wildcardServlets.end()) {
        m_wildcardServlets.erase(it);
    }
}

}  // namespace zmuduo::net::http::ws