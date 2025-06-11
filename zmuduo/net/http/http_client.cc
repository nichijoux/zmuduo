#include "zmuduo/net/http/http_client.h"
#include <memory>
#include <utility>

#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/common_util.h"
#include "zmuduo/net/http/http_context.h"
#include "zmuduo/net/http/http_parser.h"

namespace zmuduo::net::http {
HttpClient::HttpClient(EventLoop* loop, const std::string& uri, std::string name)
    : HttpClient(loop, utils::CommonUtil::CheckNotNull(Uri::Create(uri)), std::move(name)) {}

HttpClient::HttpClient(EventLoop* loop, const Uri& uri, std::string name)

    : HttpClient(loop, uri.createAddress(), std::move(name)) {
    assert(uri.getScheme() == "http" || uri.getScheme() == "https");
    m_path = uri.getPath();
    m_host = uri.getHost();
#ifdef ZMUDUO_ENABLE_OPENSSL
    if (!m_host.empty()) {
        m_client.setSSLHostName(m_host);
    }
#endif
}

HttpClient::HttpClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name)
    : m_client(loop, serverAddress, std::move(name)),
      m_host(serverAddress->toString()),
      m_path(),
      m_reconnect(true),
      m_callbacks() {
    m_client.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    m_client.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void HttpClient::doGet(const std::string&           path,
                       HttpResponseCallback         callback,
                       const HttpClient::HeaderMap& headers,
                       const std::string&           body,
                       uint32_t                     timeout) {
    return doRequest(HttpMethod::GET, path, std::move(callback), headers, body, timeout);
}

void HttpClient::doPost(const std::string&           path,
                        HttpResponseCallback         callback,
                        const HttpClient::HeaderMap& headers,
                        const std::string&           body,
                        uint32_t                     timeout) {
    return doRequest(HttpMethod::POST, path, std::move(callback), headers, body, timeout);
}

void HttpClient::doPut(const std::string&           path,
                       HttpResponseCallback         callback,
                       const HttpClient::HeaderMap& headers,
                       const std::string&           body,
                       uint32_t                     timeout) {
    return doRequest(HttpMethod::PUT, path, std::move(callback), headers, body, timeout);
}

void HttpClient::doDelete(const std::string&           path,
                          HttpResponseCallback         callback,
                          const HttpClient::HeaderMap& headers,
                          const std::string&           body,
                          uint32_t                     timeout) {
    return doRequest(HttpMethod::DELETE, path, std::move(callback), headers, body, timeout);
}

void HttpClient::doRequest(HttpMethod                   method,
                           const std::string&           path,
                           HttpResponseCallback         callback,
                           const HttpClient::HeaderMap& headers,
                           const std::string&           body,
                           uint32_t                     timeout) {
    HttpRequest request;
    // 设置方法
    request.setMethod(method);
    // 设置url
    request.setPath(m_path + path);
    request.setClose(false);
    // 如果headers中不存在host则设置host
    request.setHeader("Host", m_host);
    // 设置headers
    for (const auto& [k, v] : headers) {
        if (strcasecmp(k.c_str(), "host") == 0) {
            continue;
        }

        request.setHeader(k, v);
    }
    // 设置body
    request.setBody(body);
    // 实际发送请求
    doRequest(request, std::move(callback), timeout);
}

void HttpClient::doRequest(const HttpRequest&   request,
                           HttpResponseCallback callback,
                           uint32_t             timeout) {
    // 添加一个请求
    m_callbacks.emplace(request, std::move(callback), timeout);
    if (!m_client.isConnected()) {
        // 没有连接则连接
        m_client.connect();
    }
}

void HttpClient::onConnection(const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_FMT_DEBUG("%s -> %s is %s", connection->getLocalAddress()->toString().c_str(),
                         connection->getPeerAddress()->toString().c_str(),
                         connection->isConnected() ? "UP" : "DOWN");
    if (connection->isConnected()) {
        connection->setContext(std::make_shared<HttpContext>());
        // 发送请求
        const auto& node = m_callbacks.front();
        connection->send(std::get<0>(node).toString());
        auto timeout = std::get<2>(node);
        if (timeout != 0) {
            m_timerId = std::make_unique<TimerId>(
                m_client.getEventLoop()->runAfter(timeout, [this]() { m_client.disconnect(); }));
        }
    } else {
        // 连接关闭
        if (connection->getContext().has_value()) {
            auto  context = std::any_cast<HttpContext::Ptr>(connection->getContext());
            auto& parser  = context->getResponseParser();
            if (parser.needForceFinish()) {
                auto response = parser.getResponse();
                if (response.getHeader("Content-Length").empty() &&
                    response.getHeader("Transfer-Encoding").empty()) {
                    parser.forceFinish();
                    // 获取回调
                    auto item = m_callbacks.front();
                    m_callbacks.pop();
                    std::get<1>(item)(std::make_optional<HttpResponse>(context->getResponse()));
                }
            }
        }
        if (m_reconnect && !m_callbacks.empty()) {
            m_client.getEventLoop()->queueInLoop([this]() { m_client.connect(); });
        } else {
            // 对于剩余的回调成功的直接清空
            while (!m_callbacks.empty()) {
                auto item = m_callbacks.front();
                m_callbacks.pop();
                std::get<1>(item)(std::nullopt);
            }
        }
    }
}

void HttpClient::onMessage(const TcpConnectionPtr& connection,
                           Buffer&                 buffer,
                           const Timestamp&        receiveTime) {
needParse:
    auto context = std::any_cast<HttpContext::Ptr>(connection->getContext());
    // 解析响应
    int code = context->parseResponse(buffer);
    if (code == 1) {
        if (m_timerId) {
            m_client.getEventLoop()->cancel(*m_timerId);
            m_timerId.reset();
        }
        // 解析完成了,则从取出回调
        auto item = m_callbacks.front();
        m_callbacks.pop();
        // 获取响应
        auto response = context->getResponse();
        // 发送下一个请求
        if (!m_callbacks.empty() && !response.isClose()) {
            m_client.send(std::get<0>(item).toString());
            // 定时器
            auto timeout = std::get<2>(item);
            if (timeout != 0) {
                m_timerId = std::make_unique<TimerId>(m_client.getEventLoop()->runAfter(
                    timeout, [this]() { m_client.disconnect(); }));
            }
        }
        // 回调
        std::get<1>(item)(std::make_optional<HttpResponse>(response));
        // 重置context
        connection->setContext(std::make_shared<HttpContext>());
        // 是否需要关闭
        if (response.isClose() || m_callbacks.empty()) {
            connection->shutdown();
        } else if (buffer.getReadableBytes() > 0) {
            goto needParse;
        }
    } else if (code == -1) {
        // 存在错误
        ZMUDUO_LOG_ERROR << context->getResponseParser().getError();
        // 重置context
        connection->setContext(std::make_shared<HttpContext>());
        // 关闭连接,后面会自己移除队列
        connection->shutdown();
    }
}

}  // namespace zmuduo::net::http