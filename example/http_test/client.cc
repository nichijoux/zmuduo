#include "zmuduo/base/utils/hash_util.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/http_client.h>
#include <zmuduo/net/http/http_parser.h>
#include <zmuduo/net/http/http_server.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::thread;
using namespace zmuduo::net;
using namespace zmuduo::net::http;

int main() {
    EventLoop loop;
    auto      address = IPv4Address::Create("127.0.0.1", 8000);
    ZMUDUO_LOG_IMPORTANT << address->toString();
    HttpClient client(&loop, address, "client");
    client.connect();
    client.setConnectedCallback([&client]() {
        client.doGet("/hello", [](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
        });
        client.doGet("/hello/1", [](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
        });
    });
    loop.loop();
    return 0;
}