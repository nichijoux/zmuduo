#include "zmuduo/net/http/ws/ws_servlet.h"
#include "zmuduo/net/http/ws/ws_frame.h"
#include "zmuduo/net/tcp_connection.h"

namespace zmuduo::net::http::ws {

NotFoundServlet::NotFoundServlet() : Servlet("NotFound") {}

void NotFoundServlet::handle(const WSFrameMessage& message, TcpConnectionPtr connection) {
    // x03xE8是状态码->对应状态码1000
    static std::string data =
        WSFrameMessage(WSFrameHead::CLOSE, "\x03\xE8WebSocket Not Found Servlet").serialize(false);
    // 发送数据
    connection->send(data);
}

}  // namespace zmuduo::net::http::ws