#include <zmuduo/base/logger.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/rpc/registry_server.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::rpc;

int main(int argc, char* argv[]) {
    uint16_t  port = 8500;
    EventLoop loop;
    auto      listenAddr = IPv4Address::Create("127.0.0.1", port);

    RegistryServer server(&loop, listenAddr);
    server.start();

    ZMUDUO_LOG_INFO << "RegistryServer started on port " << port;
    loop.loop();

    return 0;
}