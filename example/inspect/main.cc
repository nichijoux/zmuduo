#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/event_loop_thread.h"

#include "inspector.h"
#include "zmuduo/base/logger.h"

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::inspect;

int main() {
    EventLoop       loop;
    EventLoopThread t;
    auto            address = IPv4Address::Create("127.0.0.1", 12345);
    Inspector       inspector(t.startLoop(), address, "inspector");
    inspector.start();
    loop.loop();
}
