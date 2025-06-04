#include "zmuduo/net/http/http_server.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/string_util.h"
#include "zmuduo/net/http/http_context.h"

using namespace zmuduo::utils;

namespace zmuduo::net::http {
HttpServer::HttpServer(EventLoop*          loop,
                       const Address::Ptr& listenAddress,
                       const std::string&  name,
                       bool                keepAlive /* = false */,
                       bool                reusePort /* = false */)
    : m_server(loop, listenAddress, name, reusePort), m_keepAlive(keepAlive) {
    m_server.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    m_server.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void HttpServer::start() {
    ZMUDUO_LOG_FMT_IMPORTANT("HttpServer[%s] starts listening on %s", m_server.getName().c_str(),
                             m_server.getIpPort().c_str());
    m_server.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_FMT_DEBUG("%s -> %s is %s", connection->getLocalAddress()->toString().c_str(),
                         m_server.getIpPort().c_str(), connection->isConnected() ? "UP" : "DOWN");
    if (connection->isConnected()) {
        // 已经成功建立连接
        connection->setContext(std::make_shared<HttpContext>());
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& connection,
                           Buffer&                 buffer,
                           const Timestamp&        receiveTime) {
needParse:
    // 获取上下文
    auto context = std::any_cast<HttpContext::Ptr>(connection->getContext());
    // 根据servlet进行匹配
    int parseResult = context->parseRequest(buffer);
    if (parseResult == 1) {
        // 请求解析成功
        auto& request  = context->getRequest();
        auto& response = context->getResponse();
        // url解码
        request.setPath(StringUtil::UrlDecode(request.getPath()));
        // 设置version
        response.setVersion(request.getVersion());
        response.setClose(request.isClose());
        response.setHeader("Server", m_server.getName());
        // 分发请求
        m_dispatcher.handle(request, response);
        if (!m_keepAlive || request.isClose()) {
            // 如果需要关闭则设置写入完成后的回调
            connection->setWriteCompleteCallback(
                [](const TcpConnectionPtr& connection) { connection->shutdown(); });
        }
        // 发送请求并重置context
        connection->send(response.toString());
        connection->setContext(std::make_shared<HttpContext>());
        if (!(!m_keepAlive || request.isClose()) && buffer.getReadableBytes() > 0) {
            // 如果还有数据则尝试解析
            goto needParse;
        }
    } else if (parseResult == -1) {
        // 解析存在错误
        connection->forceClose();
    }
}

}  // namespace zmuduo::net::http