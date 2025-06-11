#include "zmuduo/base/utils/hash_util.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/ws/ws_server.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;
using namespace zmuduo::net::http::ws;

int main() {
    EventLoop    loop;
    Address::Ptr address = IPv4Address::Create("127.0.0.1", 8000);
    WSServer     server(&loop, address, "name");
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
}