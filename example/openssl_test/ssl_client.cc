#include <zmuduo/base/logger.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    EventLoop loop;
    auto      addr = IPv4Address::Create("127.0.0.1", 8000);
    TcpClient client(&loop, addr, "SSLClient");
    if (client.loadCertificates("cacert.pem", "privkey.pem")) {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
    } else {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
    }
    client.setConnectionCallback(
        [](const auto& connection) { connection->send("I'm a ssl client"); });
    client.setMessageCallback(
        [](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp&) {
            auto msg = buffer.retrieveAllAsString();
            ZMUDUO_LOG_FMT_IMPORTANT("收到消息 %s", msg.c_str());
            connection->send(msg);
        });

    client.connect();
    loop.loop();
}