#include "zmuduo/net/http/http_context.h"

namespace zmuduo::net::http {
HttpContext::HttpContext() {}

int HttpContext::parseRequest(Buffer& buffer) {
    return m_requestParser.parse(buffer);
}

int HttpContext::parseResponse(Buffer& buffer) {
    return m_responseParser.parse(buffer);
}
} // namespace zmuduo::net::http