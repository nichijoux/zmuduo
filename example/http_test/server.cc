#include "zmuduo/base/utils/hash_util.h"
#include <zmuduo/base/logger.h>
#include <zmuduo/net/http/http_parser.h>
#include <zmuduo/net/http/http_server.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::thread;
using namespace zmuduo::net;
using namespace zmuduo::net::http;

int main() {
    EventLoop  loop;
    auto       address = IPv4Address::Create("127.0.0.1", 8000);
    HttpServer server(&loop, address, "HttpServer", true);
    auto&      dispatcher = server.getServletDispatcher();
    dispatcher.addExactServlet("/hello", [](const HttpRequest& request, HttpResponse& response) {
        ZMUDUO_LOG_IMPORTANT << "收到请求:\n" << request;
        response.setStatus(HttpStatus::OK);
        response.setBody("hello");
    });
    dispatcher.addWildcardServlet(
        "/hello/*", [](const HttpRequest& request, HttpResponse& response) {
            ZMUDUO_LOG_IMPORTANT << "收到请求:\n" << request;
            response.setStatus(HttpStatus::OK);
            response.setBody("hello the world\nthe query is " + request.getQuery() +
                             "\n the body is " + request.getBody());
        });
    server.start();
    loop.loop();
    return 0;
}