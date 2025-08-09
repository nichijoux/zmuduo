#include "zmuduo/net/http/ws/ws_server.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/base/utils/string_util.h"
#include "zmuduo/net/http/http_context.h"
#include "zmuduo/net/http/http_parser.h"

using namespace zmuduo::utils::hash_util;
using namespace zmuduo::utils::string_util;

namespace zmuduo::net::http::ws {
WSServer::WSServer(EventLoop* loop, const Address::Ptr& listenAddress, const std::string& name)
    : m_server(loop, listenAddress, name), m_dispatcher(), m_connections() {
    m_server.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    m_server.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void WSServer::start() {
    ZMUDUO_LOG_FMT_IMPORTANT("WSServer[%s] starts listening on %s", m_server.getName().c_str(),
                             m_server.getIpPort().c_str());
    m_server.start();
}

void WSServer::onConnection(const TcpConnectionPtr& connection) {
    if (connection->isConnected()) {
        ZMUDUO_LOG_DEBUG << "[WSServer] Connection UP : "
                         << connection->getPeerAddress()->toString();
        TcpConnection* it = connection.get();
        if (m_connections.find(it) != m_connections.end()) {
            ZMUDUO_LOG_ERROR << "something error, [" << it->getName() << "] exist";
            return;
        }
        m_connections[it] =
            std::make_tuple(State::TCP, "/", std::make_shared<WSFrameParser>(), nullptr);
        connection->setContext(std::make_shared<HttpContext>());
    } else {
        ZMUDUO_LOG_DEBUG << "[WSServer] Connection DOWN : "
                         << connection->getPeerAddress()->toString();
        TcpConnection* it = connection.get();
        m_connections.erase(it);
    }
}

void WSServer::onMessage(const TcpConnectionPtr& connection,
                         Buffer&                 buffer,
                         const Timestamp&        receiveTime) {
    TcpConnection* it = connection.get();
    if (m_connections.find(it) == m_connections.end()) {
        ZMUDUO_LOG_ERROR << "something error, [" << it->getName() << "] not exist";
        return;
    }
    State state = std::get<0>(m_connections[it]);
    if (state == State::TCP) {
        auto context     = std::any_cast<HttpContext::Ptr>(connection->getContext());
        auto parseStatus = context->parseRequest(buffer);
        if (parseStatus == -1) {
            connection->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            connection->shutdown();
        } else if (parseStatus == 1) {
            httpHandShake(connection, context->getRequest());
            context.reset();
        }
    } else if (state == State::WEBSOCKET) {
        onWSCommunication(connection, buffer);
    } else {
        ZMUDUO_LOG_ERROR << "[" << it->getName()
                         << "] state error, state = " << static_cast<int>(state);
        connection->forceClose();
    }
}

void WSServer::httpHandShake(const TcpConnectionPtr& connection, const HttpRequest& request) {
    auto context = std::any_cast<HttpContext::Ptr>(connection->getContext());

    TcpConnection* it = connection.get();
    // connection 必须为Upgrade
    if (strcasecmp(request.getHeader("Connection").c_str(), "Upgrade") != 0) {
        ZMUDUO_LOG_ERROR << "[WSServer] http request header's Connection field isn't Upgrade";
        connection->forceClose();
        return;
    }
    // upgrade必须为websocket
    if (strcasecmp(request.getHeader("Upgrade").c_str(), "websocket") != 0) {
        ZMUDUO_LOG_ERROR << "[WSServer] http request header's Upgrade field isn't websocket";
        connection->forceClose();
        return;
    }
    // 获取websocket-key
    std::string key = request.getHeader("Sec-WebSocket-Key");
    if (key.empty()) {
        ZMUDUO_LOG_ERROR << "[WSServer] http request Sec-WebSocket-Key is nullptr";
        // 获取websocket-key失败则强制关闭
        connection->forceClose();
        return;
    }
    auto& response = context->getResponse();
    // 判断是否为子协议
    std::string subProtocols = request.getHeader("Sec-WebSocket-Protocol");
    if (!subProtocols.empty()) {
        // 先去掉\r\t\n,然后再按;分割
        const auto& protocol =
            selectSubProtocol(Split(Trim(subProtocols), ';'));
        if (protocol) {
            // 成功选择了一个子协议
            response.setHeader("Sec-WebSocket-Protocol", protocol->getName());
            std::get<3>(m_connections[it]) = protocol;
        } else {
            ZMUDUO_LOG_ERROR << m_server.getName() << " not support the sub protocol: "
                             << request.getHeader("Sec-WebSocket-Protocol");
            connection->forceClose();
            return;
        }
    }
    // 设置版本和是否关闭(m_close不起作用,因为后面设置了websocket)
    response.setVersion(request.getVersion());
    response.setClose(true);
    // 设置状态和原因
    response.setStatus(HttpStatus::SWITCHING_PROTOCOLS);
    response.setReason(HttpStatusToString(http::HttpStatus::SWITCHING_PROTOCOLS));
    response.setWebSocket(true);
    // 添加header
    response.setHeader("Upgrade", "websocket");
    response.setHeader("Connection", "Upgrade");
    response.setHeader(
        "Sec-WebSocket-Accept",
        Base64encode(HexToBinary(SHA1sum(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"))));
    // 成功升级为websocket,将其加入到集合中
    std::get<0>(m_connections[it]) = State::WEBSOCKET;
    std::get<1>(m_connections[it]) = request.getPath();
    // 发送消息
    connection->send(response.toString());
}

void WSServer::onWSCommunication(const TcpConnectionPtr& connection, Buffer& buffer) {
    TcpConnection* it   = connection.get();
    auto           item = m_connections[it];
needParse:
    const auto& parser = std::get<2>(item);
    // 解析数据
    int code = parser->parse(buffer, true);
    if (code == 1) {
        auto& wsFrame = parser->getWSFrameMessage();
        // 设置选择的子协议
        const_cast<WSFrameMessage&>(wsFrame).m_subProtocol = std::get<3>(item);
        if (wsFrame.isControlFrame()) {
            // 使用onWSFrameControl接管控制帧率
            handleWSFrameControl(connection, wsFrame, true);
        } else {
            // 解析成功
            m_dispatcher.handle(std::get<1>(item), wsFrame, connection);
        }
        // 重置解析器
        std::get<2>(item) = std::make_shared<WSFrameParser>();
        // 如果buffer中仍有剩余的数据,则尝试再次解析
        if (buffer.getReadableBytes() > 0) {
            goto needParse;
        }
    } else if (code == -1) {
        buffer.retrieveAll();
        // 解析失败,存在error,则发送CLOSE帧
        connection->send(
            WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, parser->getErrorMessage())
                .serialize(false));
    }
}

WSSubProtocol::Ptr WSServer::selectSubProtocol(const std::vector<std::string>& subProtocols) {
    for (const auto& name : subProtocols) {
        // 查找支持的子协议
        auto it = std::find_if(m_subProtocols.begin(), m_subProtocols.end(),
                               [name](const WSSubProtocol::Ptr& protocol) {
                                   return protocol && protocol->getName() == name;
                               });
        if (it != m_subProtocols.end()) {
            return *it;
        }
    }
    return nullptr;
}
}  // namespace zmuduo::net::http::ws