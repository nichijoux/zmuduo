#include "json_sub_protocol.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/ws/ws_server.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;
using namespace zmuduo::net::http::ws;

void testBasicWebSocket() {
    EventLoop    loop;
    Address::Ptr address = IPv4Address::Create("127.0.0.1", 8000);
    WSServer     server(&loop, address, "WSServer[Basic]");
    server.getServletDispatcher().addExactServlet("/echo", [](const WSFrameMessage&   message,
                                                              const TcpConnectionPtr& connection) {
        ZMUDUO_LOG_INFO << "received: " << message.m_payload;
        connection->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, "You said:" + message.m_payload)
                             .serialize(false));
    });
    server.getServletDispatcher().addExactServlet("/", [](const WSFrameMessage&   message,
                                                          const TcpConnectionPtr& connection) {
        connection->send(WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, "我就想关闭")
                             .serialize(false));
    });
    server.start();
    loop.loop();
}

void testWebSocketWithOpenSSL() {
#ifdef ZMUDUO_ENABLE_OPENSSL
    EventLoop    loop;
    Address::Ptr address = IPv4Address::Create("127.0.0.1", 8000);
    WSServer     server(&loop, address, "WSServer[OpenSSL]");
    // 加载证书
    if (server.loadCertificates("cacert.pem", "privkey.pem")) {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
    } else {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
    }
    server.getServletDispatcher().addExactServlet("/echo", [](const WSFrameMessage&   message,
                                                              const TcpConnectionPtr& connection) {
        ZMUDUO_LOG_INFO << "received: " << message.m_payload;
        connection->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, "You said:" + message.m_payload)
                             .serialize(false));
    });
    server.getServletDispatcher().addExactServlet("/", [](const WSFrameMessage&   message,
                                                          const TcpConnectionPtr& connection) {
        connection->send(WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, "我就想关闭")
                             .serialize(false));
    });
    server.start();
    loop.loop();
#else
    ZMUDUO_LOG_ERROR << "Not enable openssl";
#endif
}

void testWebSocketWithSubProtocol() {
    EventLoop    loop;
    Address::Ptr address = IPv4Address::Create("127.0.0.1", 8000);
    WSServer     server(&loop, address, "WSServer[SubProtocol]");
    // 添加支持的子协议
    server.addSupportSubProtocol(std::make_shared<JsonWSSubProtocol>());
    server.getServletDispatcher().addExactServlet("/echo", [](const WSFrameMessage&   message,
                                                              const TcpConnectionPtr& connection) {
        if (message.m_subProtocol) {
            ZMUDUO_LOG_INFO << "received: "
                            << std::any_cast<JSON>(
                                   message.m_subProtocol->process(message.m_payload));
            Person person{"Hello World",
                          "You said:" + to_string(std::any_cast<JSON>(
                                            message.m_subProtocol->process(message.m_payload))),
                          100};
            JSON   p = person;
            connection->send(WSFrameMessage(WSFrameHead::BIN_FRAME, to_string(p)).serialize(false));
        }
    });
    server.getServletDispatcher().addExactServlet("/", [](const WSFrameMessage&   message,
                                                          const TcpConnectionPtr& connection) {
        connection->send(WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, "我就想关闭")
                             .serialize(false));
    });
    server.start();
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