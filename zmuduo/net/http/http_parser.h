// Copyright (c) nichijoux[2025] (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_PARSER_H
#define ZMUDUO_NET_HTTP_HTTP_PARSER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/http/http11_parser.h"
#include "zmuduo/net/http/http_core.h"
#include "zmuduo/net/http/httpclient_parser.h"

namespace zmuduo::net::http {
/**
 * @class HttpRequestParser
 * @brief HTTP 请求解析器。
 *
 * HttpRequestParser 用于解析 HTTP 请求数据，基于 http-parser
 * 库，支持解析方法、路径、查询参数、片段、头部和消息体。<br/> 使用状态机管理解析流程，适合 Web
 * 服务器处理客户端请求。
 *
 * @note HttpRequestParser 不可拷贝（继承自 NoCopyable），通过智能指针（Ptr）管理实例。
 * @note 解析操作是非阻塞的，依赖 Buffer 提供输入数据。
 *
 * @example
 *
 * @code
 * HttpRequestParser::Ptr parser = std::make_shared<HttpRequestParser>();
 * Buffer buffer;
 * buffer.append("GET /api?query=1 HTTP/1.1\r\nHost: example.com\r\n\r\n");
 * int result = parser->parse(buffer);
 * if (result == 1) {
 *     const HttpRequest& request = parser->getRequest();
 *     std::cout << request.toString() << std::endl;
 * } else if (result == -1) {
 *     std::cerr << parser->getError() << std::endl;
 * }
 * @endcode
 */
class HttpRequestParser : NoCopyable {
public:
    /**
     * @typedef std::shared_ptr&lt;HttpRequestParser&gt;
     * @brief HttpRequestParser 智能指针类型。
     */
    using Ptr = std::shared_ptr<HttpRequestParser>;

public:
    /**
     * @brief 构造函数，初始化解析器。
     * @note 初始化状态为 WAIT_HEAD，并设置回调函数。
     */
    HttpRequestParser();

    /**
     * @brief 析构函数，清理资源。
     */
    ~HttpRequestParser() = default;

    /**
     * @brief 解析 HTTP 请求数据。
     * @param[in,out] buffer 输入缓冲区，包含待解析的数据。
     * @retval 1 解析成功，请求完整，getRequest() 可获取结果。
     * @retval 0 数据不足，需继续提供数据进行解析。
     * @retval -1 解析出错，错误信息可通过 getError() 获取。
     */
    int parse(Buffer& buffer);

    /**
     * @brief 获取解析后的 HTTP 请求。
     * @return 解析完成的 HttpRequest 对象。
     */
    const HttpRequest& getRequest() const {
        return m_request;
    }

    /**
     * @brief 获取解析错误信息。
     * @return 错误信息字符串，若无错误则为空。
     */
    const std::string& getError() const {
        return m_error;
    }

private:
    /**
     * @enum State
     * @brief HTTP 请求解析状态。
     */
    enum class State {
        WAIT_HEAD, ///< 等待解析头部
        WAIT_BODY, ///< 等待解析消息体
        FINISH,    ///< 解析完成
        ERROR      ///< 解析出错
    };

    /**
     * @brief 处理 WAIT_HEAD 状态，解析请求头部。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleWaitHeadState(Buffer& buffer);

    /**
     * @brief 处理 WAIT_BODY 状态，解析消息体。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleWaitBodyState(Buffer& buffer);

private:
    State       m_state;      ///< 当前解析状态
    std::string m_error;      ///< 解析错误信息
    http_parser m_parser;     ///< 底层 HTTP 解析器
    HttpRequest m_request;    ///< 解析后的请求对象
    Buffer      m_buffer;     ///< 内部缓冲区
    size_t      m_dataLength; ///< 剩余待解析的消息体长度
};

/**
 * @class HttpResponseParser
 * @brief HTTP 响应解析器。
 *
 * HttpResponseParser 用于解析 HTTP 响应数据，基于 httpclient_parser
 * 库，支持解析状态码、原因短语、头部和消息体（包括 Content-Length 和 chunked 编码）。<br/>
 * 使用状态机管理解析流程，适合 HTTP 客户端处理服务器响应。
 *
 * @note HttpResponseParser 不可拷贝（继承自 NoCopyable），通过智能指针（Ptr）管理实例。
 * @note 解析操作是非阻塞的，依赖 Buffer 提供输入数据。
 *
 * @example
 *
 * @code
 * HttpResponseParser::Ptr parser = std::make_unique<HttpResponseParser>();
 * Buffer buffer;
 * buffer.append("HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!");
 * int result = parser->parse(buffer);
 * if (result == 1) {
 *     const HttpResponse& response = parser->getResponse();
 *     std::cout << response.toString() << std::endl;
 * } else if (result == -1) {
 *     std::cerr << parser->getError() << std::endl;
 * }
 * @endcode
 */
class HttpResponseParser : NoCopyable {
public:
    /**
     * @typedef std::unique_ptr&lt;HttpResponseParser&gt;
     * @brief HttpResponseParser 智能指针类型。
     */
    using Ptr = std::unique_ptr<HttpResponseParser>;

public:
    /**
     * @brief 构造函数，初始化解析器。
     * @note 初始化状态为 WAIT_HEAD，并设置回调函数。
     */
    HttpResponseParser();

    /**
     * @brief 析构函数，清理资源。
     */
    ~HttpResponseParser() = default;

    /**
     * @brief 解析 HTTP 响应数据。
     * @param[in,out] buffer 输入缓冲区，包含待解析的数据。
     * @retval 1 解析成功，响应完整，getResponse() 可获取结果。
     * @retval 0 数据不足，需继续提供数据进行解析。
     * @retval -1 解析出错，错误信息可通过 getError() 获取。
     */
    int parse(Buffer& buffer);

    /**
     * @brief 检查是否需要强制完成解析。
     * @retval true 响应没有 Content-Length 或 Transfer-Encoding，需要强制完成。
     * @retval false 响应有明确的长度或分块编码，无需强制完成。
     */
    bool needForceFinish() const {
        return m_state == State::NO_CONTENT_LENGTH;
    }

    /**
     * @brief 强制完成解析。
     * @note 用于没有 Content-Length 且非 chunked 编码的响应，将缓冲区内容设置为消息体。
     */
    void forceFinish() {
        m_state = State::FINISH;
        m_response.setBody(m_buffer.retrieveAllAsString());
    }

    /**
     * @brief 获取解析后的 HTTP 响应。
     * @return 解析完成的 HttpResponse 对象。
     */
    const HttpResponse& getResponse() const {
        return m_response;
    }

    /**
     * @brief 获取解析错误信息。
     * @return 错误信息字符串，若无错误则为空。
     */
    const std::string& getError() const {
        return m_error;
    }

private:
    /**
     * @enum State
     * @brief HTTP 响应解析状态。
     */
    enum class State {
        WAIT_HEAD,         ///< 等待解析头部
        WAIT_BODY,         ///< 等待解析消息体
        NO_CONTENT_LENGTH, ///< 无 Content-Length
        CONTENT_LENGTH,    ///< 有 Content-Length
        CHUNKED_ENCODING,  ///< chunked 编码
        FINISH,            ///< 解析完成
        ERROR              ///< 解析出错
    };

    /**
     * @brief 处理 WAIT_HEAD 状态，解析响应头部。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleWaitHeadState(Buffer& buffer);

    /**
     * @brief 处理 WAIT_BODY 状态，确定消息体解析方式。
     */
    void handleWaitBodyState();

    /**
     * @brief 处理 NO_CONTENT_LENGTH 情况，收集数据直到强制完成。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleNoContentLengthCase(Buffer& buffer);

    /**
     * @brief 处理 CONTENT_LENGTH 情况，解析固定长度消息体。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleContentLengthCase(Buffer& buffer);

    /**
     * @brief 处理 CHUNKED_ENCODING 情况，解析分块编码消息体。
     * @param[in,out] buffer 输入缓冲区。
     */
    void handleChunkedEncodingCase(Buffer& buffer);

    /**
     * @brief 设置解析错误信息并更新状态。
     * @param[in] error 错误信息。
     */
    void setParseError(std::string error) {
        m_error = std::move(error);
        m_state = State::ERROR;
    }

private:
    State             m_state;      ///< 当前解析状态
    std::string       m_error;      ///< 解析错误信息
    httpclient_parser m_parser;     ///< 底层 HTTP 客户端解析器
    HttpResponse      m_response;   ///< 解析后的响应对象
    Buffer            m_buffer;     ///< 内部缓冲区
    size_t            m_dataLength; ///< 剩余待解析的消息体长度
};
} // namespace zmuduo::net::http

#endif