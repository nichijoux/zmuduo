#include "daytime.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/net/event_loop.h"

#include <unistd.h>

using namespace zmuduo::net;

int main() {
    EventLoop     loop;
    auto          listenAddr = IPv4Address::Create("127.0.0.1", 8000);
    DaytimeServer server(&loop, listenAddr);
    ZMUDUO_LOG_FMT_INFO("pid = %d, DaytimeServer[%s]", getpid(), listenAddr->toString().c_str());
    server.start();
    loop.loop();
}
