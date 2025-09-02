#include "zmuduo/net/http/http_servlet.h"

#include <memory>

namespace zmuduo::net::http {
NotFoundServlet::NotFoundServlet(std::string name)
    : Servlet(std::move(name)) {
    m_content = "<html><head><title>404 Not Found"
                "</title></head><body><center><h1>404 Not Found</h1></center>"
                "<hr><center>" +
                name + "</center></body></html>";
}

void NotFoundServlet::handle(const HttpRequest& request, HttpResponse& response) {
    response.setStatus(HttpStatus::NOT_FOUND);
    response.setHeader("Server", "zmuduo/1.0.0");
    response.setHeader("Content-Type", "text/html");
    response.setBody(m_content);
}
} // namespace zmuduo::net::http