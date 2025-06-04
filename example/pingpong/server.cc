#include "zmuduo/net/tcp_server.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/base/types.h"
#include "zmuduo/base/utils/system_util.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"

#include <cstdio>
#include <unistd.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::thread;

void onConnection(const TcpConnectionPtr& conn) {
    if (conn->isConnected()) {
        conn->setTcpNoDelay(true);
    }
}

void onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp&) {
    conn->send(buf);
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: server <address> <port> <threads>\n");
    } else {
        ZMUDUO_LOG_FMT_INFO("pid =  %d, tid = %d", utils::SystemUtil::GetPid(),
                            utils::SystemUtil::GetThreadId());

        const char* ip          = argv[1];
        auto        port        = lexical_cast<uint16_t>(argv[2]);
        auto        listenAddr  = IPv4Address::Create(ip, port);
        auto        threadCount = lexical_cast<int>(argv[3]);

        EventLoop loop;

        TcpServer server(&loop, listenAddr, "PingPong");

        server.setConnectionCallback(onConnection);
        server.setMessageCallback(onMessage);

        if (threadCount > 1) {
            server.setThreadNum(threadCount);
        }

        server.start();

        loop.loop();
    }
}
