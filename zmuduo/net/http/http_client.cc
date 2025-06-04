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
}

HttpClient::HttpClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name)
    : m_client(loop, serverAddress, std::move(name)),
      m_host(serverAddress->toString()),
      m_path(),
      m_connectCallback(nullptr),
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
                       const std::string&           body) {
    return doRequest(HttpMethod::GET, path, std::move(callback), headers, body);
}

void HttpClient::doPost(const std::string&           path,
                        HttpResponseCallback         callback,
                        const HttpClient::HeaderMap& headers,
                        const std::string&           body) {
    return doRequest(HttpMethod::POST, path, std::move(callback), headers, body);
}

void HttpClient::doPut(const std::string&           path,
                       HttpResponseCallback         callback,
                       const HttpClient::HeaderMap& headers,
                       const std::string&           body) {
    return doRequest(HttpMethod::PUT, path, std::move(callback), headers, body);
}

void HttpClient::doDelete(const std::string&           path,
                          HttpResponseCallback         callback,
                          const HttpClient::HeaderMap& headers,
                          const std::string&           body) {
    return doRequest(HttpMethod::DELETE, path, std::move(callback), headers, body);
}

void HttpClient::doRequest(HttpMethod                   method,
                           const std::string&           path,
                           HttpResponseCallback         callback,
                           const HttpClient::HeaderMap& headers,
                           const std::string&           body) {
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
        if (strcasecmp(k.c_str(), "connection") == 0) {
            continue;
        }

        if (strcasecmp(k.c_str(), "host") == 0) {
            continue;
        }

        request.setHeader(k, v);
    }
    // 设置body
    request.setBody(body);
    // 实际发送请求
    doRequest(request, std::move(callback));
}

void HttpClient::doRequest(const HttpRequest& request, HttpResponseCallback callback) {
    m_client.send(request.toString());
    m_callbacks.emplace(std::move(callback));
}

void HttpClient::onConnection(const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_FMT_DEBUG("%s -> %s is %s", connection->getLocalAddress()->toString().c_str(),
                         connection->getPeerAddress()->toString().c_str(),
                         connection->isConnected() ? "UP" : "DOWN");
    if (connection->isConnected()) {
        // 连接成功
        if (m_connectCallback) {
            m_connectCallback();
        }
        connection->setContext(std::make_shared<HttpContext>());
    } else {
        // 连接关闭
        auto  context = std::any_cast<HttpContext::Ptr>(connection->getContext());
        auto& parser  = context->getResponseParser();
        if (parser.needForceFinish()) {
            auto response = parser.getResponse();
            if (response.getHeader("Content-Length").empty() &&
                response.getHeader("Transfer-Encoding").empty()) {
                parser.forceFinish();
                // 获取回调
                auto callback = m_callbacks.front();
                m_callbacks.pop();
                callback(std::make_shared<HttpResponse>(context->getResponse()));
            }
        }
        // 对于剩余的回调成功的直接清空
        while (!m_callbacks.empty()) {
            auto callback = m_callbacks.front();
            m_callbacks.pop();
            callback(nullptr);
        }
    }
}

void HttpClient::onMessage(const TcpConnectionPtr& connection,
                           Buffer&                 buffer,
                           const Timestamp&        receiveTime) {
    ZMUDUO_LOG_FMT_DEBUG("%s received data", receiveTime.toString().data());
needParse:
    auto context = std::any_cast<HttpContext::Ptr>(connection->getContext());
    // 解析响应
    int code = context->parseResponse(buffer);
    if (code == 1) {
        // 解析完成了,则从取出回调
        auto callback = m_callbacks.front();
        m_callbacks.pop();
        auto response = context->getResponse();
        callback(std::make_shared<HttpResponse>(response));
        // 重置context
        connection->setContext(std::make_shared<HttpContext>());
        // 是否需要关闭
        if (response.isClose()) {
            connection->shutdown();
        } else if (buffer.getReadableBytes() > 0) {
            goto needParse;
        }
    } else if (code == -1) {
        // 存在错误
        ZMUDUO_LOG_ERROR << context->getResponseParser().getError();
        // 有错误,则清除所有的response
        while (!m_callbacks.empty()) {
            auto callback = m_callbacks.front();
            m_callbacks.pop();
            callback(nullptr);
        }
        connection->shutdown();
        // 重置context
        connection->setContext(std::make_shared<HttpContext>());
    }
}

}  // namespace zmuduo::net::http