#include "zmuduo/net/http/http_parser.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/types.h"
#include <charconv>

namespace zmuduo::net::http {
#define GET_REQUEST const_cast<HttpRequest&>(parser->getRequest())
/// http request

static void onRequestMethod(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    auto  method = CharsToHttpMethod(at);
    // 解析出的方法不正确
    if (method == HttpMethod::INVALID_METHOD) {
        ZMUDUO_LOG_FMT_WARNING("Invalid http request method: %s", std::string(at, length).c_str());
        return;
    }
    GET_REQUEST.setMethod(method);
}

static void onRequestUri(void* data, const char* at, size_t length) {}

static void onRequestFragment(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    GET_REQUEST.setFragment(std::string(at, length));
}

static void onRequestPath(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    GET_REQUEST.setPath(std::string(at, length));
}

static void onRequestQuery(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    GET_REQUEST.setQuery(std::string(at, length));
}

static void onRequestVersion(void* data, const char* at, size_t length) {
    auto*   parser = static_cast<HttpRequestParser*>(data);
    uint8_t version;
    if (strncmp(at, "HTTP/1.1", length) == 0) {
        version = 0x11;
    } else if (strncmp(at, "HTTP/1.0", length) == 0) {
        version = 0x10;
    } else {
        ZMUDUO_LOG_FMT_WARNING("Invalid http request version: %s", std::string(at, length).c_str());
        return;
    }
    GET_REQUEST.setVersion(version);
}

static void onRequestHeaderDone(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    GET_REQUEST.setBody(std::string(at, length));
}

static void
onRequestHttpField(void* data, const char* field, size_t flen, const char* value, size_t vlen) {
    auto* parser = static_cast<HttpRequestParser*>(data);
    if (flen == 0) {
        ZMUDUO_LOG_FMT_WARNING("Invalid http request field length == 0");
        return;
    }
    GET_REQUEST.setHeader(std::string(field, flen), std::string(value, vlen));
}
#undef GET_REQUEST

/// http response

#define GET_RESPONSE const_cast<HttpResponse&>(parser->getResponse())

static void onResponseReason(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpResponseParser*>(data);
    GET_RESPONSE.setReason(std::string(at, length));
}

static void onResponseStatus(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpResponseParser*>(data);
    int   status;
    std::from_chars(at, at + length, status);
    GET_RESPONSE.setStatus(static_cast<HttpStatus>(status));
}

static void onResponseChunkSize(void* data, const char* at, size_t length) {
    auto* parser = static_cast<HttpResponseParser*>(data);
    int   size;
    std::from_chars(at, at + length, size, 16);
    // +2 跳过chunk大小后的\r\n
    GET_RESPONSE.setBody(GET_RESPONSE.getBody() + std::string(at + length + 2, size));
}

static void onResponseVersion(void* data, const char* at, size_t length) {
    auto*   parser = static_cast<HttpResponseParser*>(data);
    uint8_t version;
    if (strncmp(at, "HTTP/1.1", length) == 0) {
        version = 0x11;
    } else if (strncmp(at, "HTTP/1.0", length) == 0) {
        version = 0x10;
    } else {
        ZMUDUO_LOG_FMT_WARNING("Invalid http request version: %s", std::string(at, length).c_str());
        return;
    }
    GET_RESPONSE.setVersion(version);
}

static void onResponseHeaderDone(void* data, const char* at, size_t length) {}

static void onResponseLastChunk(void* data, const char* at, size_t length) {}

static void
onResponseHttpField(void* data, const char* field, size_t flen, const char* value, size_t vlen) {
    auto* parser = static_cast<HttpResponseParser*>(data);
    if (flen == 0) {
        ZMUDUO_LOG_FMT_WARNING("Invalid http response field length == 0");
        return;
    }
    GET_RESPONSE.setHeader(std::string(field, flen), std::string(value, vlen));
}
#undef GET_RESPONSE

}  // namespace zmuduo::net::http

namespace zmuduo::net::http {
HttpRequestParser::HttpRequestParser()
    : m_state(State::WAIT_HEAD), m_dataLength(0), m_error(), m_parser(), m_request() {
    http_parser_init(&m_parser);
    // 初始化回调
    m_parser.request_method = onRequestMethod;
    m_parser.request_uri    = onRequestUri;
    m_parser.fragment       = onRequestFragment;
    m_parser.request_path   = onRequestPath;
    m_parser.query_string   = onRequestQuery;
    m_parser.http_version   = onRequestVersion;
    m_parser.header_done    = onRequestHeaderDone;
    m_parser.http_field     = onRequestHttpField;
    m_parser.data           = this;
}

int HttpRequestParser::parse(Buffer& buffer) {
    State beforeState;
    do {
        beforeState = m_state;
        switch (m_state) {
            case State::WAIT_HEAD: handleWaitHeadState(buffer); break;
            case State::WAIT_BODY: handleWaitBodyState(buffer); break;
            case State::FINISH:
            case State::ERROR: break;
        }
    } while (beforeState != m_state);
    return m_state == State::FINISH ? 1 : m_error.empty() ? 0 : -1;
}

void HttpRequestParser::handleWaitHeadState(Buffer& buffer) {
    auto footer = buffer.findHeaderFooter();
    // 只解析头部
    if (!footer) {
        return;
    }
    auto length = footer - buffer.peek() + strlen(Buffer::S_HEADER_FOOTER);
    // 解析头部是否完成
    http_parser_execute(&m_parser, buffer.peek(), length, 0);
    int code = http_parser_finish(&m_parser);
    if (code == 1) {
        // 如果为1,则查看能否获取content-length
        auto contentLength = m_request.getHeaderAs<size_t>("Content-Length");
        // 如果有content-length且不为0,且大于body长度,则说明body没解析完毕
        if (contentLength != 0) {
            // body没解析完毕
            m_dataLength = contentLength;
            m_state      = State::WAIT_BODY;
        } else {
            // body也解析完毕,说明完全解析了
            m_state = State::FINISH;
        }
        buffer.retrieve(length);
    } else if (code == -1) {
        // 存在错误
        m_state = State::ERROR;
        m_error = "解析错误";
    }
}

void HttpRequestParser::handleWaitBodyState(Buffer& buffer) {
    // 这是body没解析(或者没解析完全)
    // 可获取的数据长度
    size_t length = std::min(buffer.getReadableBytes(), m_dataLength);
    m_request.setBody(m_request.getBody() + std::string(buffer.peek(), length));
    buffer.retrieve(length);
    m_dataLength -= length;
    if (m_dataLength == 0) {
        m_state = State::FINISH;
    }
}

HttpResponseParser::HttpResponseParser()
    : m_parser(), m_response(), m_error(), m_state(State::WAIT_HEAD), m_dataLength(0) {
    httpclient_parser_init(&m_parser);
    m_parser.reason_phrase = onResponseReason;
    m_parser.status_code   = onResponseStatus;
    m_parser.chunk_size    = onResponseChunkSize;
    m_parser.http_version  = onResponseVersion;
    m_parser.header_done   = onResponseHeaderDone;
    m_parser.last_chunk    = onResponseLastChunk;
    m_parser.http_field    = onResponseHttpField;
    m_parser.data          = this;
}

int HttpResponseParser::parse(Buffer& buffer) {
    State beforeState;
    do {
        beforeState = m_state;
        switch (m_state) {
            case State::WAIT_HEAD: handleWaitHeadState(buffer); break;
            case State::WAIT_BODY: handleWaitBodyState(); break;
            case State::NO_CONTENT_LENGTH: handleNoContentLengthCase(buffer); break;
            case State::CONTENT_LENGTH: handleContentLengthCase(buffer); break;
            case State::CHUNKED_ENCODING: handleChunkedEncodingCase(buffer); break;
            case State::FINISH:
                // 解析完成了,设置body
                m_response.setBody(m_buffer.retrieveAllAsString());
                break;
            case State::ERROR: break;
        }
    } while (beforeState != m_state);
    return m_state == State::FINISH ? 1 : m_error.empty() ? 0 : -1;
}

void HttpResponseParser::handleWaitHeadState(Buffer& buffer) {
    auto footer = buffer.findHeaderFooter();
    // 存在footer,说明http协议头已经接收完全,否则说明一定不存在
    if (!footer) {
        return;
    }
    // 最大解析长度
    auto length = footer - buffer.peek() + strlen(Buffer::S_HEADER_FOOTER);
    // 实际解析长度
    httpclient_parser_execute(&m_parser, buffer.peek(), length, 0);
    // 是否解析完成
    int code = httpclient_parser_finish(&m_parser);
    if (code == 1) {
        buffer.retrieve(length);
        m_state = State::WAIT_BODY;
    } else if (code == -1) {
        setParseError("解析http响应头错误");
    }
}

void HttpResponseParser::handleWaitBodyState() {
    if (strcasecmp(m_response.getHeader("Connection").c_str(), "upgrade") == 0) {
        // 如果是升级连接
        m_state = State::FINISH;
    } else if (m_response.getHeader("Content-Length").empty() &&
               m_response.getHeader("Transfer-Encoding").empty()) {
        // 响应既没有content-length也没有transfer-encoding
        m_state = State::NO_CONTENT_LENGTH;
    } else if (m_response.getHeader("Transfer-Encoding").empty()) {
        m_dataLength = m_response.getHeaderAs<size_t>("Content-Length");
        m_state      = State::CONTENT_LENGTH;
    } else {
        m_state = State::CHUNKED_ENCODING;
    }
}

void HttpResponseParser::handleNoContentLengthCase(Buffer& buffer) {
    auto length = buffer.getReadableBytes();
    m_buffer.write(buffer.peek(), length);
    buffer.retrieve(length);
}

void HttpResponseParser::handleContentLengthCase(Buffer& buffer) {
    // 本次需要解析的长度
    size_t length = std::min(buffer.getReadableBytes(), m_dataLength);
    m_dataLength -= length;
    m_buffer.write(buffer.peek(), length);
    buffer.retrieve(length);
    if (m_dataLength == 0) {
        // 解析完毕
        m_state = State::FINISH;
    }
}

void HttpResponseParser::handleChunkedEncodingCase(Buffer& buffer) {
    // 上一次落下的长度不足的
    if (m_dataLength != 0 && buffer.getReadableBytes() >= m_dataLength + 2) {
        m_buffer.write(buffer.peek(), m_dataLength);
        buffer.retrieve(m_dataLength + 2);
        m_dataLength = 0;
    }
    // 解析chunk
    while (buffer.getReadableBytes() > 0) {
        auto crlf = buffer.findCRLF();
        if (!crlf) {
            break;
        }
        // 获取数据的大小
        size_t size;
        std::from_chars(buffer.peek(), crlf, size, 16);
        // 如果不为0则说明还没结束
        if (size != 0) {
            // 跳过数据大小和\r\n
            buffer.retrieve(crlf - buffer.peek() + 2);
            // 查看数据是否完整
            if (buffer.getReadableBytes() >= size + 2) {
                m_buffer.write(buffer.peek(), size);
                buffer.retrieve(size + 2);
            } else {
                m_dataLength = size;
                break;
            }
        } else {
            // 为0了
            if (buffer.findHeaderFooter() != nullptr) {
                // 为0并且找到了\r\n\r\n
                buffer.retrieve(5);
                m_state = State::FINISH;
                break;
            } else {
                m_state = State::ERROR;
                setParseError("解析chunk错误");
                break;
            }
        }
    }
}

}  // namespace zmuduo::net::http