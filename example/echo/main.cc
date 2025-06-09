#include "echo.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/net/event_loop.h"

#include "zmuduo/base/utils/system_util.h"
#include <unistd.h>

int main() {
    EventLoop loop;
    auto      listenAddr = IPv4Address::Create("127.0.0.1", 8000);
    ZMUDUO_LOG_FMT_INFO("pid = %d,address is %s", getpid(), listenAddr->toString().c_str());
    AsyncLogger::GetInstance().setLogMode(LogMode::BOTH);
    EchoServer server(&loop, listenAddr);
    server.start();
    loop.loop();
}
