#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/http_client.h>
#include <zmuduo/net/http/http_server.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::thread;
using namespace zmuduo::net;
using namespace zmuduo::net::http;

void testPython() {
    EventLoop loop;
    auto      address = IPv4Address::Create("127.0.0.1", 8000);
    ZMUDUO_LOG_IMPORTANT << address->toString();
    HttpClient client(&loop, address, "client");
    client.doGet(
        "/normal",
        [](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
        },
        {}, "", 5);
    client.doGet("/chunked", [](const auto& ptr) {
        if (ptr) {
            ZMUDUO_LOG_IMPORTANT << ptr->toString();
        }
    });
    client.doGet(
        "/normal",
        [&loop](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
        },
        {}, "", 5);
    client.doGet(
        "/normal",
        [&loop](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
        },
        {}, "", 5);
    loop.loop();
}

void testMuduo() {
    EventLoop loop;
    auto      address = IPv4Address::Create("127.0.0.1", 8000);
    ZMUDUO_LOG_IMPORTANT << address->toString();
    HttpClient client(&loop, address, "client");
    client.doGet("/hello", [](const auto& ptr) {
        if (ptr) {
            ZMUDUO_LOG_IMPORTANT << ptr->toString();
        }
    });
    client.doGet(
        "/hello/2",
        [&loop](const auto& ptr) {
            if (ptr) {
                ZMUDUO_LOG_IMPORTANT << ptr->toString();
            }
            loop.quit();
        },
        {}, "", 5);
    loop.loop();
}

void testBaidu() {
    EventLoop  loop;
    HttpClient client(&loop, "https://www.baidu.com/", "client");
    if (client.createSSLContext()) {
        client.doGet("/",
                     [&loop](const auto& ptr) {
                         if (ptr) {
                             ZMUDUO_LOG_IMPORTANT << ptr->toString();
                         }
                         loop.runAfter(2, [&loop]() { loop.quit(); });
                     },
                     {{"User-Agent", "ZmuduoClient"}, {"Accept", "*/*"}, {"Connection", "Close"}});
    }

    loop.loop();
}

void printUsage(const char* progName) {
    ZMUDUO_LOG_INFO << "Usage: " << progName << " <option>";
    ZMUDUO_LOG_INFO << "Options:";
    ZMUDUO_LOG_INFO << "  1    Test Python server    (GET /chunked and /normal)";
    ZMUDUO_LOG_INFO << "  2    Test Muduo server     (GET /hello and /hello/2)";
    ZMUDUO_LOG_INFO << "  3    Test Baidu homepage   (GET / from www.baidu.com)";
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printUsage(argv[0]);
        return 1;
    }
    int type = lexical_cast<int>(argv[1]);
    switch (type) {
        case 1: testPython(); break;
        case 2: testMuduo(); break;
        case 3: testBaidu(); break;
        default: ZMUDUO_LOG_WARNING << "type can be only 1-3";
    }
    return 0;
}