#include "zmuduo/net/http/http_core.h"

namespace zmuduo::net::http {
HttpMethod StringToHttpMethod(const std::string& s) {
#define XX(num, name, string)                                                                      \
    if (strcmp(#string, s.c_str()) == 0) {                                                         \
        return HttpMethod::name;                                                                   \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

HttpMethod CharsToHttpMethod(const char* s) {
#define XX(num, name, string)                                                                      \
    if (strncmp(s, #string, strlen(#string)) == 0) {                                               \
        return HttpMethod::name;                                                                   \
    }
    HTTP_METHOD_MAP(XX)
#undef XX
    return HttpMethod::INVALID_METHOD;
}

const char* HttpMethodToString(const HttpMethod& m) {
    switch (m) {
#define XX(num, name, string)                                                                      \
    case HttpMethod::name: return #string;
        HTTP_METHOD_MAP(XX)
#undef XX
        default: return "<unknown>";
    }
}

const char* HttpStatusToString(const HttpStatus& s) {
    switch (s) {
#define XX(code, name, msg)                                                                        \
    case HttpStatus::name: return #msg;
        HTTP_STATUS_MAP(XX)
#undef XX
        default: return "<unknown>";
    }
}

HttpRequest::HttpRequest(const uint8_t version, const bool close)
    : m_method(HttpMethod::GET),
      m_version(version),
      m_close(close),
      m_websocket(false),
      m_path("/") {}

std::string HttpRequest::getHeader(const std::string& key, const std::string& def) const {
    const auto it = m_headers.find(key);
    return it != m_headers.end() ? it->second : def;
}

void HttpRequest::setHeader(const std::string& key, const std::string& value) {
    m_headers[key] = value;
    // 同步字段
    if (strcasecmp(key.c_str(), "connection") == 0) {
        m_close = strcasecmp(value.c_str(), "close") == 0;
    } else if (strcasecmp(key.c_str(), "upgrade") == 0) {
        m_websocket = strcasecmp(value.c_str(), "websocket") == 0;
    }
}

std::string HttpRequest::getParam(const std::string& key, const std::string& def) const {
    const auto it = m_params.find(key);
    return it != m_params.end() ? it->second : def;
}

std::string HttpRequest::getCookie(const std::string& key, const std::string& def) const {
    const auto it = m_cookies.find(key);
    return it != m_cookies.end() ? it->second : def;
}

std::string HttpRequest::toString() const {
    std::stringstream ss;
    ss << HttpMethodToString(m_method) << " " << m_path << (m_query.empty() ? "" : "?") << m_query
        << (m_fragment.empty() ? "" : "#") << m_fragment << " HTTP/" << static_cast<uint32_t>(
            m_version >> 4)
        << "." << static_cast<uint32_t>(m_version & 0x0F) << "\r\n";
    // websocket
    if (!m_websocket) {
        ss << "Connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }
    // headers
    for (const auto& [k, v] : m_headers) {
        if (!m_websocket && strcasecmp(k.c_str(), "connection") == 0) {
            continue;
        }
        ss << k << ": " << v << "\r\n";
    }
    // cookies
    if (!m_cookies.empty()) {
        ss << "Cookies: ";
        for (const auto& [k, v] : m_cookies) {
            ss << k << "=" << v;
        }
        ss << "\r\n";
    }
    // body
    if (!m_body.empty()) {
        ss << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
    } else {
        ss << "\r\n";
    }
    return ss.str();
}

HttpResponse::HttpResponse(const uint8_t version, const bool close)
    : m_status(HttpStatus::OK),
      m_version(version),
      m_close(close),
      m_websocket(false) {}

std::string HttpResponse::getHeader(const std::string& key, const std::string& def) const {
    const auto it = m_headers.find(key);
    return it != m_headers.end() ? it->second : def;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    m_headers[key] = value;
    if (strcasecmp(key.c_str(), "connection") == 0) {
        m_close = strcasecmp(value.c_str(), "close") == 0;
    } else if (strcasecmp(key.c_str(), "upgrade") == 0) {
        m_websocket = strcasecmp(value.c_str(), "websocket") == 0;
    }
}

std::string HttpResponse::toString() const {
    std::stringstream ss;
    ss << "HTTP/" << static_cast<uint32_t>(m_version >> 4) << '.' << static_cast<uint32_t>(
            m_version & 0x0F) << ' '
        << static_cast<uint32_t>(m_status) << ' ' << (m_reason.empty() ?
                                                          HttpStatusToString(m_status) :
                                                          m_reason)
        << "\r\n";

    if (!m_websocket) {
        ss << "Connection: " << (m_close ? "close" : "keep-alive") << "\r\n";
    }

    for (const auto& [k, v] : m_headers) {
        if (!m_websocket && strcasecmp(k.c_str(), "connection") == 0) {
            continue;
        }
        ss << k << ": " << v << "\r\n";
    }

    for (auto& i : m_cookies) {
        ss << "Set-Cookie: " << i << "\r\n";
    }
    // 响应内容body
    if (!m_body.empty()) {
        ss << "content-length: " << m_body.size() << "\r\n\r\n" << m_body;
    } else {
        ss << "\r\n";
    }
    return ss.str();
}
} // namespace zmuduo::net::http