// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_CONTEXT_H
#define ZMUDUO_NET_HTTP_HTTP_CONTEXT_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/http/http_parser.h"
#include <memory>

namespace zmuduo::net::http {
/**
 * @class HttpContext
 * @brief HTTP 协议上下文管理类。
 *
 * HttpContext 封装 HTTP 请求和响应的解析功能，管理 HttpRequestParser 和 HttpResponseParser。<br/>
 * 提供统一的解析接口和结果访问方法。适用于 Web 服务器或客户端的 HTTP 协议处理，符合 HTTP/1.1
 * 标准（RFC 7230-7235）。
 *
 * @note HttpContext 不可拷贝（继承自 NoCopyable），通过智能指针（Ptr）管理实例。
 * @note 解析操作是非阻塞的，依赖 Buffer 提供输入数据，调用者需确保 Buffer 的线程安全。
 *
 * @example
 *
 * @code
 * HttpContext::Ptr context = std::make_shared<HttpContext>();
 * Buffer buffer;
 * buffer.append("GET /api?query=1 HTTP/1.1\r\nHost: example.com\r\n\r\n");
 * int result = context->parseRequest(buffer);
 * if (result == 1) {
 *     const HttpRequest& request = context->getRequest();
 *     std::cout << request.toString() << std::endl;
 * } else if (result == -1) {
 *     std::cerr << context->getRequestParser().getError() << std::endl;
 * }
 * @endcode
 */
class HttpContext : NoCopyable {
public:
    /**
     * @typedef std::shared_ptr&lt;HttpContext&gt;
     * @brief HttpContext 智能指针类型。
     */
    using Ptr = std::shared_ptr<HttpContext>;

public:
    /**
     * @brief 构造函数。
     * @note 初始化 HTTP 请求解析器和响应解析器。
     */
    HttpContext();

    /**
     * @brief 解析 HTTP 请求。
     * @param[in,out] buffer 输入输出缓冲区，包含待解析的 HTTP 请求数据。
     * @retval 1 解析成功，请求完整，getRequest() 可获取结果。
     * @retval 0 数据不足，需继续提供数据进行解析。
     * @retval -1 解析出错，错误信息可通过 getRequestParser().getError() 获取。
     * @note 修改 buffer 内容，移除已解析的数据。调用者需确保 buffer 的线程安全。
     */
    int parseRequest(Buffer& buffer);

    /**
     * @brief 解析 HTTP 响应。
     * @param[in,out] buffer 输入输出缓冲区，包含待解析的 HTTP 响应数据。
     * @retval 1 解析成功，响应完整，getResponse() 可获取结果。
     * @retval 0 数据不足，需继续提供数据进行解析。
     * @retval -1 解析出错，错误信息可通过 getResponseParser().getError() 获取。
     * @note 修改 buffer 内容，移除已解析的数据。调用者需确保 buffer 的线程安全。
     */
    int parseResponse(Buffer& buffer);

    /**
     * @brief 获取 HTTP 请求解析器。
     * @return HTTP 请求解析器引用。
     * @note 可用于访问解析器状态或错误信息。
     */
    HttpRequestParser& getRequestParser() {
        return m_requestParser;
    }

    /**
     * @brief 获取 HTTP 响应解析器。
     * @return HTTP 响应解析器引用。
     * @note 可用于访问解析器状态或错误信息。
     */
    HttpResponseParser& getResponseParser() {
        return m_responseParser;
    }

    /**
     * @brief 获取当前 HTTP 请求对象（const 版本）。
     * @return HTTP 请求对象常量引用。
     * @note 用于只读访问解析后的请求数据。
     */
    const HttpRequest& getRequest() const {
        return m_requestParser.getRequest();
    }

    /**
     * @brief 获取当前 HTTP 请求对象（非 const 版本）。
     * @return HTTP 请求对象引用。
     * @note 用于修改解析后的请求数据，需谨慎使用。
     */
    HttpRequest& getRequest() {
        return const_cast<HttpRequest&>(m_requestParser.getRequest());
    }

    /**
     * @brief 获取当前 HTTP 响应对象（const 版本）。
     * @return HTTP 响应对象常量引用。
     * @note 用于只读访问解析后的响应数据。
     */
    const HttpResponse& getResponse() const {
        return m_responseParser.getResponse();
    }

    /**
     * @brief 获取当前 HTTP 响应对象（非 const 版本）。
     * @return HTTP 响应对象引用。
     * @note 用于修改解析后的响应数据，需谨慎使用。
     */
    HttpResponse& getResponse() {
        return const_cast<HttpResponse&>(m_responseParser.getResponse());
    }

private:
    HttpRequestParser  m_requestParser;  ///< HTTP请求解析器
    HttpResponseParser m_responseParser; ///< HTTP响应解析器
};
} // namespace zmuduo::net::http

#endif