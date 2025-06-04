#include <zmuduo/base/logger.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      address = IPv4Address::Create("127.0.0.1", 8000);
    TcpClient client(&loop, address, "client");
    client.setConnectionCallback(defaultConnectionCallback);
    client.setMessageCallback([](const auto& connection, auto& buffer, auto& p3) {
        ZMUDUO_LOG_INFO << buffer.retrieveAllAsString();
    });
    client.connect();
    loop.loop();
    return 0;
}