#include "json_sub_protocol.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/ws/ws_client.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;
using namespace zmuduo::net::http::ws;

void testBasicWebSocket() {
    EventLoop loop;
    WSClient  client(&loop, "wss://127.0.0.1:8000/", "WSClient[Basic]");
    // 成功握手的回调
    client.setWSConnectionCallback([&](bool connect) {
        if (connect) {
            ZMUDUO_LOG_IMPORTANT << "连接成功";
            client.sendWSFrameMessage(WSFrameHead::TEXT_FRAME, "hello");
        } else {
            ZMUDUO_LOG_IMPORTANT << "断开连接";
        }
    });
    // 接收数据的回调
    client.setWSMessageCallback(
        [](const TcpConnectionPtr& connection, const WSFrameMessage& message) {
            ZMUDUO_LOG_IMPORTANT << "收到数据:" << message.m_payload;
        });
    client.connect();
    // 5秒后重连
    loop.runAfter(5, [&client]() {
        ZMUDUO_LOG_WARNING << "重新连接";
        client.connect();
    });
    loop.loop();
}

void testWebSocketWithOpenSSL() {
    EventLoop loop;
    WSClient  client(&loop, "wss://127.0.0.1:8000/echo", "WSClient[OpenSSL]");
    if (client.createSSLContext() && client.loadCustomCACertificate("cacert.pem", "")) {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
    } else {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
    }
    // 成功握手的回调
    client.setWSConnectionCallback([&](bool connect) {
        if (connect) {
            ZMUDUO_LOG_IMPORTANT << "连接成功";
            client.sendWSFrameMessage(WSFrameHead::TEXT_FRAME, "hello");
        } else {
            ZMUDUO_LOG_IMPORTANT << "断开连接";
        }
    });
    // 接收数据的回调
    client.setWSMessageCallback(
        [](const TcpConnectionPtr& connection, const WSFrameMessage& message) {
            ZMUDUO_LOG_IMPORTANT << "收到数据:" << message.m_payload;
        });
    client.connect();
    loop.loop();
}

void testWebSocketWithSubProtocol() {
    EventLoop loop;
    WSClient  client(&loop, "wss://127.0.0.1:8000/echo", "WSClient[SubProtocol]");
    client.addSupportSubProtocol(std::make_shared<JsonWSSubProtocol>());
    // 成功握手的回调
    client.setWSConnectionCallback([&](bool connect) {
        if (connect) {
            ZMUDUO_LOG_IMPORTANT << "连接成功";
            Person person{"Ned Flanders", "744 Evergreen Terrace", 60};
            JSON   j = person;
            client.sendWSFrameMessage(WSFrameHead::BIN_FRAME, to_string(j));
        } else {
            ZMUDUO_LOG_IMPORTANT << "断开连接";
        }
    });
    // 接收数据的回调
    client.setWSMessageCallback(
        [](const TcpConnectionPtr& connection, const WSFrameMessage& message) {
            if (message.m_subProtocol) {
                ZMUDUO_LOG_IMPORTANT
                    << "收到数据:"
                    << std::any_cast<JSON>(message.m_subProtocol->process(message.m_payload));
            }
        });
    client.connect();
    loop.loop();
}

void printUsage(const char* progName) {
    ZMUDUO_LOG_INFO << "Usage: " << progName << " <option>";
    ZMUDUO_LOG_INFO << "Options:";
    ZMUDUO_LOG_INFO << "  1    Test Basic WebSocket            (GET /)";
    ZMUDUO_LOG_INFO << "  2    Test Websocket With OpenSSL     (GET /echo)";
    ZMUDUO_LOG_INFO << "  3    Test Websocket With SubProtocol (GET /echo)";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }
    int type = lexical_cast<int>(argv[1]);
    switch (type) {
        case 1: testBasicWebSocket(); break;
        case 2: testWebSocketWithOpenSSL(); break;
        case 3: testWebSocketWithSubProtocol(); break;
        default: ZMUDUO_LOG_WARNING << "type can be only 1-3";
    }
    return 0;
}