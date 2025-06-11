#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/ws/ws_client.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;
using namespace zmuduo::net::http::ws;

int main() {
    EventLoop loop;
    WSClient  client(&loop, "wss://127.0.0.1:8000/", "WSClient");
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
    // 5秒后重连
    loop.runAfter(5, [&client]() {
        ZMUDUO_LOG_WARNING << "重新连接";
        client.connect();
    });
    loop.loop();
}