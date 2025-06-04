#include "zmuduo/base/logger.h"
#include "zmuduo/net/timer_queue.h"
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/udp_client.h>

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      serverAddress = IPv4Address::Create("127.0.0.1", 8000);
    UdpClient client(&loop, serverAddress, AF_INET, "UdpClient");
    // 启动client
    client.start();
    client.setMessageCallback([](auto& client, auto& inputBuffer) {
        ZMUDUO_LOG_IMPORTANT << inputBuffer.retrieveAllAsString();
    });
    // equals
    // loop.runEvery(2.5, [&client]() { client.send(Timestamp::Now().toString()); });
    TimerQueue timerQueue(&loop);
    timerQueue.addTimer([&client]() { client.send(Timestamp::Now().toString()); },
                        Timestamp::Now(), 2.5);
    loop.loop();
    return 0;
}