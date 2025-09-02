#include "zmuduo/net/http/ws/ws_client.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/common_util.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/net/http/http_core.h"
#include <utility>

using namespace zmuduo::utils::hash_util;

namespace zmuduo::net::http::ws {
WSClient::WSClient(EventLoop* loop, const std::string& uri, std::string name)
    : WSClient(loop, utils::common_util::CheckNotNull(Uri::Create(uri)), std::move(name)) {}

WSClient::WSClient(EventLoop* loop, const Uri& uri, std::string name)
    : WSClient(loop, uri.createAddress(), std::move(name)) {
    assert(uri.getScheme() == "ws" || uri.getScheme() == "wss");
    m_path = uri.getPath();
#ifdef ZMUDUO_ENABLE_OPENSSL
    m_client.setSSLHostName(uri.getHost());
#endif
}

WSClient::WSClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name)
    : m_client(loop, serverAddress, std::move(name)) {
    // 注册回调函数
    m_client.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    m_client.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void WSClient::connect() {
    m_parser.reset();
    m_client.connect();
}

void WSClient::stop() {
    m_client.stop();
    m_state = State::NONE;
}

void WSClient::disconnect() {
    m_client.disconnect();
    m_state = State::NONE;
}

void WSClient::sendWSFrameMessage(const WSFrameMessage& message) {
    if (m_state == State::WEBSOCKET) {
        // 将数据转为可发送的二进制数据并发送
        m_client.send(message.serialize(true));
    }
}

void WSClient::doHandShake() {
    // 进行握手
    m_state = State::TCP;
    // 先生成一个key
    m_key = Base64encode(RandomString(16));
    // 然后构造http请求
    HttpRequest request;
    request.setWebsocket(true);
    request.setPath(m_path);
    request.setHeader("Upgrade", "websocket");
    request.setHeader("Connection", "Upgrade");
    request.setHeader("Sec-WebSocket-Version", "13");
    request.setHeader("Sec-WebSocket-Key", m_key);
    // 子协议
    if (!m_supportProtocols.empty()) {
        std::ostringstream oss;
        bool               first = true;
        for (const auto& subProtocol : m_supportProtocols) {
            if (!first) {
                oss << ", ";
            }
            first = false;
            oss << subProtocol->getName();
        }
        request.setHeader("Sec-WebSocket-Protocol", oss.str());
    }
    // 发送http请求
    m_state = State::HTTP;
    m_client.send(request.toString());
}

void WSClient::doWhenError() {
    // 发送close帧
    m_client.send(
        WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, m_parser.getErrorMessage())
        .serialize(true));
    // 重置数据
    m_state = State::NONE;
    m_parser.reset();
    // 强制关闭
    disconnect();
}

void WSClient::onConnection(const TcpConnectionPtr& connection) {
    if (connection->isConnected()) {
        // tcp连接成功,进行websocket握手
        ZMUDUO_LOG_FMT_DEBUG("[WSClient:%s] is UP", m_client.getName().data());
        doHandShake();
    } else {
        ZMUDUO_LOG_FMT_DEBUG("[WSClient:%s] is DOWN", m_client.getName().data());
        // 断开连接
        disconnect();
        m_useProtocol.reset();
        m_connectionCallback(false);
    }
}

void WSClient::onMessage(const TcpConnectionPtr& connection,
                         Buffer&                 buffer,
                         const Timestamp&        receiveTime) {
    assert(m_state == State::HTTP || m_state == State::WEBSOCKET);
    if (m_state == State::WEBSOCKET) {
    needParse:
        const int code = m_parser.parse(buffer, false);
        if (code == 1) {
            auto& wsFrame = m_parser.getWSFrameMessage();
            // 设置解析到的子协议
            const_cast<WSFrameMessage&>(wsFrame).m_subProtocol = m_useProtocol;
            // 解析到了一个帧
            if (wsFrame.isControlFrame()) {
                // 控制帧
                handleWSFrameControl(connection, wsFrame, false);
                // CLOSE帧需要断开连接
                if (wsFrame.m_opcode == WSFrameHead::CLOSE) {
                    m_state = State::NONE;
                }
            } else if (m_messageCallback) {
                // 数据帧且有回调
                m_messageCallback(connection, wsFrame);
            }
            // 重置websocket帧解析器
            m_parser.reset();
            if (buffer.getReadableBytes() > 0) {
                goto needParse;
            }
        } else if (code == -1) {
            doWhenError();
        }
    } else {
        // http,握手后半段,此时也获取了一个http响应
        HttpResponseParser parser;
        if (const int code = parser.parse(buffer); code == 1) {
            auto& response = parser.getResponse();
            if (response.getStatus() != HttpStatus::SWITCHING_PROTOCOLS) {
                ZMUDUO_LOG_ERROR << m_client.getName()
                    << " received error handshake http response\n"
                    << response;
                doWhenError();
                return;
            }
            // 服务器使用了子协议，接下来看客户端是否支持
            const auto it = std::find_if(m_supportProtocols.begin(), m_supportProtocols.end(),
                                         [protocol = response.getHeader("Sec-WebSocket-Protocol")](
                                         const WSSubProtocol::Ptr& subProtocol) {
                                             return subProtocol && protocol == subProtocol->
                                                    getName();
                                         });
            if (it != m_supportProtocols.end()) {
                // 选中了子协议
                m_useProtocol = *it;
            } else if (!response.getHeader("Sec-WebSocket-Protocol").empty()) {
                // 没有对应的子协议
                ZMUDUO_LOG_ERROR << m_client.getName() << " not support the sub protocol: "
                    << response.getHeader("Sec-WebSocket-Protocol");
                doWhenError();
                return;
            }
            // 检验http中的accept是否合法
            const auto acceptKey = response.getHeader("Sec-WebSocket-Accept");
            const auto validKey  =
                Base64encode(HexToBinary(SHA1sum(m_key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
            if (validKey != acceptKey) {
                ZMUDUO_LOG_ERROR << m_client.getName()
                    << "received invalid Sec-WebSocket-Accept: " << acceptKey;
                doWhenError();
                return;
            }
            // 切换为合理状态
            m_state = State::WEBSOCKET;
            if (m_connectionCallback) {
                m_connectionCallback(true);
            }
        } else if (code == -1) {
            // 异常
            ZMUDUO_LOG_ERROR << parser.getError();
            doWhenError();
        }
    }
}
} // namespace zmuduo::net::http::ws