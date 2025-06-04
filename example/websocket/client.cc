#include "zmuduo/base/utils/hash_util.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/ws/ws_client.h>

using namespace zmuduo;
using namespace zmuduo::thread;
using namespace zmuduo::net;
using namespace zmuduo::net::http;
using namespace zmuduo::net::http::ws;

int main() {
    const char* data = "8000/";
    EventLoop   loop;
    auto        address = IPv4Address::Create("127.0.0.1", 8000);
    WSClient    client(&loop, "ws://127.0.0.1:8000/", "WSClient");
    client.setWSConnectionCallback([&](bool connect) {
        if (connect) {
            ZMUDUO_LOG_IMPORTANT << "连接成功";
            client.sendWSFrameMessage(WSFrameHead::TEXT_FRAME, "hello");
        } else {
            ZMUDUO_LOG_IMPORTANT << "断开连接";
        }
    });
    client.setWSMessageCallback(
        [](const TcpConnectionPtr& connection, const WSFrameMessage& message) {
            ZMUDUO_LOG_IMPORTANT << "收到数据:" << message.m_payload;
        });
    client.connect();
    loop.loop();
}