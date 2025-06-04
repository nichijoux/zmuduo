#include "zmuduo/base/logger.h"
#include "zmuduo/net/udp_server.h"

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      listenAddress = IPv4Address::Create("127.0.0.1", 8000);
    UdpServer server(&loop, listenAddress, "UdpServerTest");
    server.setMessageCallback(
        [](UdpServer& server, Buffer& buffer, const Address::Ptr& peerAddress) {
            auto message = buffer.retrieveAllAsString();
            ZMUDUO_LOG_INFO << "receive message: " << message << "from " << peerAddress;
            server.send("You said: " + message, peerAddress);
        });
    server.start();
    loop.loop();
    return 0;
}