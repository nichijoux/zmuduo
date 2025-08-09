// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_HTTP_H
#define ZMUDUO_NET_HTTP_HTTP_H

#include "zmuduo/base/copyable.h"
#include "zmuduo/base/types.h"
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace zmuduo::net::http {
/**
 * @struct CaseInsensitiveLess
 * @brief 忽略大小写的字符串比较仿函数。
 *
 * 用于 std::map 的键比较，支持 HTTP 头部和参数的忽略大小写查找。
 */
struct CaseInsensitiveLess {
    /**
     * @brief 比较两个字符串，忽略大小写。
     * @param[in] lhs 左字符串。
     * @param[in] rhs 右字符串。
     * @return 如果 lhs 小于 rhs 返回 true，否则返回 false。
     */
    bool operator()(const std::string& lhs, const std::string& rhs) const {
        return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

/* Request Methods */
#define HTTP_METHOD_MAP(XX)                                                                        \
    XX(0, DELETE, DELETE)                                                                          \
    XX(1, GET, GET)                                                                                \
    XX(2, HEAD, HEAD)                                                                              \
    XX(3, POST, POST)                                                                              \
    XX(4, PUT, PUT)                                                                                \
    /* pathological */                                                                             \
    XX(5, CONNECT, CONNECT)                                                                        \
    XX(6, OPTIONS, OPTIONS)                                                                        \
    XX(7, TRACE, TRACE)                                                                            \
    /* WebDAV */                                                                                   \
    XX(8, COPY, COPY)                                                                              \
    XX(9, LOCK, LOCK)                                                                              \
    XX(10, MKCOL, MKCOL)                                                                           \
    XX(11, MOVE, MOVE)                                                                             \
    XX(12, PROPFIND, PROPFIND)                                                                     \
    XX(13, PROPPATCH, PROPPATCH)                                                                   \
    XX(14, SEARCH, SEARCH)                                                                         \
    XX(15, UNLOCK, UNLOCK)                                                                         \
    XX(16, BIND, BIND)                                                                             \
    XX(17, REBIND, REBIND)                                                                         \
    XX(18, UNBIND, UNBIND)                                                                         \
    XX(19, ACL, ACL)                                                                               \
    /* subversion */                                                                               \
    XX(20, REPORT, REPORT)                                                                         \
    XX(21, MKACTIVITY, MKACTIVITY)                                                                 \
    XX(22, CHECKOUT, CHECKOUT)                                                                     \
    XX(23, MERGE, MERGE)                                                                           \
    /* upnp */                                                                                     \
    XX(24, MSEARCH, M - SEARCH)                                                                    \
    XX(25, NOTIFY, NOTIFY)                                                                         \
    XX(26, SUBSCRIBE, SUBSCRIBE)                                                                   \
    XX(27, UNSUBSCRIBE, UNSUBSCRIBE)                                                               \
    /* RFC-5789 */                                                                                 \
    XX(28, PATCH, PATCH)                                                                           \
    XX(29, PURGE, PURGE)                                                                           \
    /* CalDAV */                                                                                   \
    XX(30, MKCALENDAR, MKCALENDAR)                                                                 \
    /* RFC-2068, section 19.6.1.2 */                                                               \
    XX(31, LINK, LINK)                                                                             \
    XX(32, UNLINK, UNLINK)                                                                         \
    /* icecast */                                                                                  \
    XX(33, SOURCE, SOURCE)

/* Status Codes */
#define HTTP_STATUS_MAP(XX)                                                                        \
    XX(100, CONTINUE, Continue)                                                                    \
    XX(101, SWITCHING_PROTOCOLS, Switching Protocols)                                              \
    XX(102, PROCESSING, Processing)                                                                \
    XX(200, OK, OK)                                                                                \
    XX(201, CREATED, Created)                                                                      \
    XX(202, ACCEPTED, Accepted)                                                                    \
    XX(203, NON_AUTHORITATIVE_INFORMATION, Non - Authoritative Information)                        \
    XX(204, NO_CONTENT, No Content)                                                                \
    XX(205, RESET_CONTENT, Reset Content)                                                          \
    XX(206, PARTIAL_CONTENT, Partial Content)                                                      \
    XX(207, MULTI_STATUS, Multi - Status)                                                          \
    XX(208, ALREADY_REPORTED, Already Reported)                                                    \
    XX(226, IM_USED, IM Used)                                                                      \
    XX(300, MULTIPLE_CHOICES, Multiple Choices)                                                    \
    XX(301, MOVED_PERMANENTLY, Moved Permanently)                                                  \
    XX(302, FOUND, Found)                                                                          \
    XX(303, SEE_OTHER, See Other)                                                                  \
    XX(304, NOT_MODIFIED, Not Modified)                                                            \
    XX(305, USE_PROXY, Use Proxy)                                                                  \
    XX(307, TEMPORARY_REDIRECT, Temporary Redirect)                                                \
    XX(308, PERMANENT_REDIRECT, Permanent Redirect)                                                \
    XX(400, BAD_REQUEST, Bad Request)                                                              \
    XX(401, UNAUTHORIZED, Unauthorized)                                                            \
    XX(402, PAYMENT_REQUIRED, Payment Required)                                                    \
    XX(403, FORBIDDEN, Forbidden)                                                                  \
    XX(404, NOT_FOUND, Not Found)                                                                  \
    XX(405, METHOD_NOT_ALLOWED, Method Not Allowed)                                                \
    XX(406, NOT_ACCEPTABLE, Not Acceptable)                                                        \
    XX(407, PROXY_AUTHENTICATION_REQUIRED, Proxy Authentication Required)                          \
    XX(408, REQUEST_TIMEOUT, Request Timeout)                                                      \
    XX(409, CONFLICT, Conflict)                                                                    \
    XX(410, GONE, Gone)                                                                            \
    XX(411, LENGTH_REQUIRED, Length Required)                                                      \
    XX(412, PRECONDITION_FAILED, Precondition Failed)                                              \
    XX(413, PAYLOAD_TOO_LARGE, Payload Too Large)                                                  \
    XX(414, URI_TOO_LONG, URI Too Long)                                                            \
    XX(415, UNSUPPORTED_MEDIA_TYPE, Unsupported Media Type)                                        \
    XX(416, RANGE_NOT_SATISFIABLE, Range Not Satisfiable)                                          \
    XX(417, EXPECTATION_FAILED, Expectation Failed)                                                \
    XX(421, MISDIRECTED_REQUEST, Misdirected Request)                                              \
    XX(422, UNPROCESSABLE_ENTITY, Unprocessable Entity)                                            \
    XX(423, LOCKED, Locked)                                                                        \
    XX(424, FAILED_DEPENDENCY, Failed Dependency)                                                  \
    XX(426, UPGRADE_REQUIRED, Upgrade Required)                                                    \
    XX(428, PRECONDITION_REQUIRED, Precondition Required)                                          \
    XX(429, TOO_MANY_REQUESTS, Too Many Requests)                                                  \
    XX(431, REQUEST_HEADER_FIELDS_TOO_LARGE, Request Header Fields Too Large)                      \
    XX(451, UNAVAILABLE_FOR_LEGAL_REASONS, Unavailable For Legal Reasons)                          \
    XX(500, INTERNAL_SERVER_ERROR, Internal Server Error)                                          \
    XX(501, NOT_IMPLEMENTED, Not Implemented)                                                      \
    XX(502, BAD_GATEWAY, Bad Gateway)                                                              \
    XX(503, SERVICE_UNAVAILABLE, Service Unavailable)                                              \
    XX(504, GATEWAY_TIMEOUT, Gateway Timeout)                                                      \
    XX(505, HTTP_VERSION_NOT_SUPPORTED, HTTP Version Not Supported)                                \
    XX(506, VARIANT_ALSO_NEGOTIATES, Variant Also Negotiates)                                      \
    XX(507, INSUFFICIENT_STORAGE, Insufficient Storage)                                            \
    XX(508, LOOP_DETECTED, Loop Detected)                                                          \
    XX(510, NOT_EXTENDED, Not Extended)                                                            \
    XX(511, NETWORK_AUTHENTICATION_REQUIRED, Network Authentication Required)

/**
 * @enum HttpMethod
 * @brief HTTP 请求方法枚举。
 *
 * 定义了标准 HTTP 方法（GET, POST 等）及扩展方法（WebDAV, CalDAV 等），符合 RFC 7231 和相关标准。
 */
enum class HttpMethod {
#define XX(num, name, string) name = (num),
    HTTP_METHOD_MAP(XX)
#undef XX
        INVALID_METHOD
};

/**
 * @enum HttpStatus
 * @brief HTTP 响应状态码枚举。
 *
 * 定义了标准 HTTP 状态码（200 OK, 404 Not Found 等），符合 RFC 7231 和相关标准。
 */
enum class HttpStatus {
#define XX(code, name, desc) name = (code),
    HTTP_STATUS_MAP(XX)
#undef XX
};

/**
 * @brief 将字符串转换为 HTTP 方法枚举。
 * @param[in] s HTTP 方法字符串（如 "GET", "POST"）。
 * @return 对应的 HttpMethod 枚举值，若无效则返回 INVALID_METHOD。
 */
HttpMethod StringToHttpMethod(const std::string& s);

/**
 * @brief 将 C 字符串转换为 HTTP 方法枚举。
 * @param[in] s C 字符串表示的 HTTP 方法。
 * @return 对应的 HttpMethod 枚举值，若无效则返回 INVALID_METHOD。
 */
HttpMethod CharsToHttpMethod(const char* s);

/**
 * @brief 将 HTTP 方法枚举转换为字符串。
 * @param[in] m HTTP 方法枚举值。
 * @return 对应的方法字符串，若无效则返回 "<unknown>"。
 */
const char* HttpMethodToString(const HttpMethod& m);

/**
 * @brief 将 HTTP 状态码枚举转换为字符串。
 * @param[in] s HTTP 状态码枚举值。
 * @return 对应的状态描述字符串，若无效则返回 "<unknown>"。
 */
const char* HttpStatusToString(const HttpStatus& s);

/**
 * @brief 从 Map 中获取键值并转换为指定类型，返回转换是否成功。
 * @tparam MapType Map 类型（如 std::map）。
 * @tparam T 目标类型。
 * @param[in] map Map 数据结构。
 * @param[in] key 键名。
 * @param[out] val 存储转换后的值。
 * @param[in] def 默认值（转换失败时使用）。
 * @retval true 键存在且转换成功，val 为转换后的值。
 * @retval false 键不存在或转换失败，val 为默认值。
 * @note 使用 lexical_cast 进行类型转换，可能抛出异常。
 */
template <class MapType, class T>
bool checkGetAs(const MapType& map, const std::string& key, T& val, const T& def = T()) {
    auto it = map.find(key);
    if (it == map.end()) {
        val = def;
        return false;
    }
    try {
        val = lexical_cast<T>(it->second);
        return true;
    } catch (...) { val = def; }
    return false;
}

/**
 * @brief 从 Map 中获取键值并转换为指定类型。
 * @tparam MapType Map 类型（如 std::map）。
 * @tparam T 目标类型。
 * @param[in] map Map 数据结构。
 * @param[in] key 键名。
 * @param[in] def 默认值（键不存在或转换失败时使用）。
 * @return 转换后的值，若键不存在或转换失败则返回默认值。
 * @note 使用 lexical_cast 进行类型转换，可能抛出异常。
 */
template <typename MapType, typename T>
T getAs(const MapType& map, const std::string& key, const T& def = T()) {
    auto it = map.find(key);
    if (it == map.end()) {
        return def;
    }
    return lexical_cast<T>(it->second, def);
}

/**
 * @class HttpRequest
 * @brief 表示 HTTP 请求。
 *
 * HttpRequest 封装了 HTTP 请求的属性，包括方法、版本、路径、查询参数、头部、Cookie 和消息体。<br/>
 * 支持 WebSocket 升级和长连接（keep-alive），符合 HTTP/1.1 标准（RFC 7230-7235）。
 *
 * @note HttpRequest 是可拷贝的（继承自 Copyable），通过智能指针（Ptr）管理实例。
 * @note 头部键名忽略大小写，使用 CaseInsensitiveLess 比较。
 *
 * @example
 *
 * @code
 * HttpRequest::Ptr request = std::make_shared<HttpRequest>(0x11, false);
 * request->setMethod(HttpMethod::POST);
 * request->setPath("/api");
 * request->setHeader("Content-Type", "application/json");
 * request->setBody("{\"key\": \"value\"}");
 * std::cout << request->toString() << std::endl;
 * @endcode
 */
class HttpRequest : public Copyable {
  public:
    /**
     * @typedef std::map&lt;std::string, std::string, CaseInsensitiveLess&gt;
     * @brief HTTP 头部和参数的 Map 类型，忽略大小写。
     */
    using Map = std::map<std::string, std::string, CaseInsensitiveLess>;
    /**
     * @typedef std::shared_ptr&lt;HttpRequest&gt;
     * @brief HttpRequest 智能指针类型。
     */
    using Ptr = std::shared_ptr<HttpRequest>;

    /**
     * @brief 构造函数。
     * @param[in] version HTTP 版本（如 0x11 表示 1.1）。
     * @param[in] close 是否关闭连接（true 表示 close，false 表示 keep-alive）。
     */
    explicit HttpRequest(uint8_t version = 0x11, bool close = true);

    /**
     * @brief 获取请求方法。
     * @return HTTP 方法枚举值。
     */
    HttpMethod getMethod() const {
        return m_method;
    }

    /**
     * @brief 设置请求方法。
     * @param[in] method HTTP 方法枚举值。
     */
    void setMethod(HttpMethod method) {
        m_method = method;
    }

    /**
     * @brief 获取 HTTP 版本。
     * @return 版本号（如 0x11 表示 1.1）。
     */
    uint8_t getVersion() const {
        return m_version;
    }

    /**
     * @brief 设置 HTTP 版本。
     * @param[in] version 版本号。
     */
    void setVersion(uint8_t version) {
        m_version = version;
    }

    /**
     * @brief 检查是否关闭连接。
     * @retval true 是 close 连接
     * @retval false 非 close连接
     */
    bool isClose() const {
        return m_close;
    }

    /**
     * @brief 设置连接是否关闭。
     * @param[in] close 是否关闭连接。
     */
    void setClose(bool close) {
        m_close = close;
    }

    /**
     * @brief 检查是否为 WebSocket 请求。
     * @return 如果是 WebSocket 请求返回 true，否则返回 false。
     */
    bool isWebsocket() const {
        return m_websocket;
    }

    /**
     * @brief 设置是否为 WebSocket 请求。
     * @param[in] websocket 是否为 WebSocket。
     */
    void setWebsocket(bool websocket) {
        m_websocket = websocket;
    }

    /**
     * @brief 获取请求路径。
     * @return 请求路径字符串。
     */
    const std::string& getPath() const {
        return m_path;
    }

    /**
     * @brief 设置请求路径。
     * @param[in] path 请求路径。
     */
    void setPath(const std::string& path) {
        m_path = path;
    }

    /**
     * @brief 获取查询参数字符串。
     * @return 查询参数字符串。
     */
    const std::string& getQuery() const {
        return m_query;
    }

    /**
     * @brief 设置查询参数字符串。
     * @param[in] query 查询参数。
     */
    void setQuery(const std::string& query) {
        m_query = query;
    }

    /**
     * @brief 获取片段标识符。
     * @return 片段标识符字符串。
     */
    const std::string& getFragment() const {
        return m_fragment;
    }

    /**
     * @brief 设置片段标识符。
     * @param[in] fragment 片段标识符。
     */
    void setFragment(const std::string& fragment) {
        m_fragment = fragment;
    }

    /**
     * @brief 获取消息体。
     * @return 消息体字符串。
     */
    const std::string& getBody() const {
        return m_body;
    }

    /**
     * @brief 设置消息体。
     * @param[in] body 消息体。
     */
    void setBody(const std::string& body) {
        m_body = body;
    }

    /**
     * @brief 获取指定头部值。
     * @param[in] key 头部键名。
     * @param[in] def 默认值（键不存在时返回）。
     * @return 头部值或默认值。
     */
    std::string getHeader(const std::string& key, const std::string& def = "") const;

    /**
     * @brief 添加头部键值对。
     * @param[in] key 头部键名。
     * @param[in] value 头部值。
     * @note 如果键为 "Connection" 或 "Upgrade"，同步更新 m_close 或 m_websocket。
     */
    void setHeader(const std::string& key, const std::string& value);

    /**
     * @brief 移除指定头部。
     * @param[in] key 头部键名。
     */
    void removeHeader(const std::string& key) {
        m_headers.erase(key);
    }

    /**
     * @brief 检查是否存在指定头部。
     * @param[in] key 头部键名。
     * @retval true 存在指定头部
     * @retval false 不存在指定头部
     */
    bool hasHeader(const std::string& key) {
        return m_headers.find(key) != m_headers.end();
    }

    /**
     * @brief 检查并获取头部值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 头部键名。
     * @param[out] val 存储转换后的值。
     * @param[in] def 默认值（转换失败时使用）。
     * @retval true 键存在且转换成功，val 为转换后的值。
     * @retval false 键不存在或转换失败，val 为默认值。
     */
    template <typename T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) const {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
     * @brief 获取头部值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 头部键名。
     * @param[in] def 默认值（键不存在或转换失败时返回）。
     * @return 转换后的值，若键不存在或转换失败则返回默认值。
     */
    template <typename T>
    T getHeaderAs(const std::string& key, const T& def = T()) const {
        return getAs(m_headers, key, def);
    }

    /**
     * @brief 获取所有头部。
     * @return 头部 Map。
     */
    const Map& getHeaders() const {
        return m_headers;
    }

    /**
     * @brief 设置所有头部。
     * @param[in] headers 头部 Map。
     */
    void setHeaders(const Map& headers) {
        m_headers = headers;
    }

    /**
     * @brief 获取指定参数值。
     * @param[in] key 参数键名。
     * @param[in] def 默认值（键不存在时返回）。
     * @return 参数值或默认值。
     */
    std::string getParam(const std::string& key, const std::string& def = "") const;

    /**
     * @brief 添加参数键值对。
     * @param[in] key 参数键名。
     * @param[in] value 参数值。
     */
    void setParam(const std::string& key, const std::string& value) {
        m_params[key] = value;
    }

    /**
     * @brief 移除指定参数。
     * @param[in] key 参数键名。
     */
    void removeParam(const std::string& key) {
        m_params.erase(key);
    }

    /**
     * @brief 检查是否存在指定参数。
     * @param[in] key 参数键名。
     * @return 如果存在返回 true，否则返回 false。
     */
    bool hasParam(const std::string& key) {
        return m_params.find(key) != m_params.end();
    }

    /**
     * @brief 检查并获取参数值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 参数键名。
     * @param[out] val 存储转换后的值。
     * @param[in] def 默认值（转换失败时使用）。
     * @retval true 键存在且转换成功，val 为转换后的值。
     * @retval false 键不存在或转换失败，val 为默认值。
     */
    template <typename T>
    bool checkGetParamAs(const std::string& key, T& val, const T& def = T()) const {
        return checkGetAs(m_params, key, val, def);
    }

    /**
     * @brief 获取参数值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 参数键名。
     * @param[in] def 默认值（键不存在或转换失败时返回）。
     * @return 转换后的值，若键不存在或转换失败则返回默认值。
     */
    template <typename T>
    T getParamAs(const std::string& key, const T& def = T()) const {
        return getAs(m_params, key, def);
    }

    /**
     * @brief 获取所有参数。
     * @return 参数 Map。
     */
    const Map& getParams() const {
        return m_params;
    }

    /**
     * @brief 设置所有参数。
     * @param[in] params 参数 Map。
     */
    void setParams(const Map& params) {
        m_params = params;
    }

    /**
     * @brief 获取指定 Cookie 值。
     * @param[in] key Cookie 键名。
     * @param[in] def 默认值（键不存在时返回）。
     * @return Cookie 值或默认值。
     */
    std::string getCookie(const std::string& key, const std::string& def = "") const;

    /**
     * @brief 添加 Cookie 键值对。
     * @param[in] key Cookie 键名。
     * @param[in] value Cookie 值。
     */
    void setCookie(const std::string& key, const std::string& value) {
        m_cookies[key] = value;
    }

    /**
     * @brief 移除指定 Cookie。
     * @param[in] key Cookie 键名。
     */
    void removeCookie(const std::string& key) {
        m_cookies.erase(key);
    }

    /**
     * @brief 检查是否存在指定 Cookie。
     * @param[in] key Cookie 键名。
     * @retval true 存在指定cookie
     * @retval false 不存在指定cookie
     */
    bool hasCookie(const std::string& key) {
        return m_cookies.find(key) != m_cookies.end();
    }

    /**
     * @brief 检查并获取 Cookie 值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key Cookie 键名。
     * @param[out] val 存储转换后的值。
     * @param[in] def 默认值（转换失败时使用）。
     * @retval true 键存在且转换成功，val 为转换后的值。
     * @retval false 键不存在或转换失败，val 为默认值。
     */
    template <typename T>
    bool checkGetCookieAs(const std::string& key, T& val, const T& def = T()) const {
        return checkGetAs(m_cookies, key, val, def);
    }

    /**
     * @brief 获取 Cookie 值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key Cookie 键名。
     * @param[in] def 默认值（键不存在或转换失败时返回）。
     * @return 转换后的值，若键不存在或转换失败则返回默认值。
     */
    template <typename T>
    T getCookieAs(const std::string& key, const T& def = T()) const {
        return getAs(m_cookies, key, def);
    }

    /**
     * @brief 获取所有 Cookie。
     * @return Cookie Map。
     */
    const Map& getCookies() const {
        return m_cookies;
    }

    /**
     * @brief 设置所有 Cookie。
     * @param[in] cookies Cookie Map。
     */
    void setCookies(const Map& cookies) {
        m_cookies = cookies;
    }

    /**
     * @brief 将请求转换为字符串。
     * @return 符合 HTTP 协议的请求字符串。
     */
    std::string toString() const;

    /**
     * @brief 输出请求到流。
     * @param[in,out] os 输出流。
     * @param[in] request HttpRequest 对象。
     * @return 输出流。
     */
    friend std::ostream& operator<<(std::ostream& os, const HttpRequest& request) {
        os << request.toString();
        return os;
    }

  private:
    HttpMethod  m_method;     ///< HTTP 方法
    uint8_t     m_version;    ///< HTTP 版本
    bool        m_close;      ///< 是否关闭连接
    bool        m_websocket;  ///< 是否为 WebSocket 请求
    std::string m_path;       ///< 请求路径
    std::string m_query;      ///< 查询参数
    std::string m_fragment;   ///< 片段标识符
    std::string m_body;       ///< 消息体
    Map         m_headers;    ///< 头部 Map
    Map         m_params;     ///< 参数 Map
    Map         m_cookies;    ///< Cookie Map
};

/**
 * @class HttpResponse
 * @brief 表示 HTTP 响应。
 *
 * HttpResponse 封装了 HTTP 响应的属性，包括状态码、版本、头部、Cookie、消息体和原因短语。<br/>
 * 支持 WebSocket 升级和长连接（keep-alive），符合 HTTP/1.1 标准（RFC 7230-7235）。
 *
 * @note HttpResponse 是可拷贝的（继承自 Copyable），通过智能指针（Ptr）管理实例。
 * @note 头部键名忽略大小写，使用 CaseInsensitiveLess 比较。
 *
 * @example
 *
 * @code
 * HttpResponse::Ptr response = std::make_shared<HttpResponse>(0x11, false);
 * response->setStatus(HttpStatus::OK);
 * response->setHeader("Content-Type", "text/plain");
 * response->setBody("Hello, World!");
 * std::cout << response->toString() << std::endl;
 * @endcode
 */
class HttpResponse : public Copyable {
  public:
    /**
     * @typedef std::map&lt;std::string, std::string, CaseInsensitiveLess&gt;
     * @brief HTTP 头部 Map 类型，忽略大小写。
     */
    using Map = std::map<std::string, std::string, CaseInsensitiveLess>;

    /**
     * @typedef std::shared_ptr&lt;HttpResponse&gt;
     * @brief HttpResponse 智能指针类型。
     */
    using Ptr = std::shared_ptr<HttpResponse>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] version HTTP 版本（如 0x11 表示 1.1）。
     * @param[in] close 是否关闭连接（true 表示 close，false 表示 keep-alive）。
     */
    explicit HttpResponse(uint8_t version = 0x11, bool close = true);

    /**
     * @brief 获取响应状态码。
     * @return HTTP 状态码枚举值。
     */
    HttpStatus getStatus() const {
        return m_status;
    }

    /**
     * @brief 设置响应状态码。
     * @param[in] status HTTP 状态码枚举值。
     */
    void setStatus(HttpStatus status) {
        m_status = status;
    }

    /**
     * @brief 获取 HTTP 版本。
     * @return 版本号（如 0x11 表示 1.1）。
     */
    uint8_t getVersion() const {
        return m_version;
    }

    /**
     * @brief 设置 HTTP 版本。
     * @param[in] version 版本号。
     */
    void setVersion(uint8_t version) {
        m_version = version;
    }

    /**
     * @brief 检查是否关闭连接。
     * @return 如果为 close 连接返回 true，否则返回 false。
     */
    bool isClose() const {
        return m_close;
    }

    /**
     * @brief 设置连接是否关闭。
     * @param[in] close 是否关闭连接。
     */
    void setClose(bool close) {
        m_close = close;
    }

    /**
     * @brief 检查是否为 WebSocket 响应。
     * @return 如果是 WebSocket 响应返回 true，否则返回 false。
     */
    bool isWebSocket() const {
        return m_websocket;
    }

    /**
     * @brief 设置是否为 WebSocket 响应。
     * @param[in] isWebsocket 是否为 WebSocket。
     */
    void setWebSocket(bool isWebsocket) {
        m_websocket = isWebsocket;
    }

    /**
     * @brief 获取消息体。
     * @return 消息体字符串。
     */
    const std::string& getBody() const {
        return m_body;
    }

    /**
     * @brief 设置消息体。
     * @param[in] body 消息体。
     */
    void setBody(const std::string& body) {
        m_body = body;
    }

    /**
     * @brief 获取原因短语。
     * @return 原因短语字符串。
     */
    const std::string& getReason() const {
        return m_reason;
    }

    /**
     * @brief 设置原因短语。
     * @param[in] reason 原因短语。
     */
    void setReason(const std::string& reason) {
        m_reason = reason;
    }

    /**
     * @brief 获取指定头部值。
     * @param[in] key 头部键名。
     * @param[in] def 默认值（键不存在时返回）。
     * @return 头部值或默认值。
     */
    std::string getHeader(const std::string& key, const std::string& def = "") const;

    /**
     * @brief 设置content-type
     * @param[in] value 值
     */
    void setContentType(const std::string& value) {
        setHeader("Content-Type", value);
    }

    /**
     * @brief 添加头部键值对。
     * @param[in] key 头部键名。
     * @param[in] value 头部值。
     * @note 如果键为 "Connection" 或 "Upgrade"，同步更新 m_close 或 m_websocket。
     */
    void setHeader(const std::string& key, const std::string& value);

    /**
     * @brief 移除指定头部。
     * @param[in] key 头部键名。
     */
    void removeHeader(const std::string& key) {
        m_headers.erase(key);
    }

    /**
     * @brief 检查并获取头部值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 头部键名。
     * @param[out] val 存储转换后的值。
     * @param[in] def 默认值（转换失败时使用）。
     * @retval true 键存在且转换成功，val 为转换后的值。
     * @retval false 键不存在或转换失败，val 为默认值。
     */
    template <class T>
    bool checkGetHeaderAs(const std::string& key, T& val, const T& def = T()) {
        return checkGetAs(m_headers, key, val, def);
    }

    /**
     * @brief 获取头部值，转换为指定类型。
     * @tparam T 目标类型。
     * @param[in] key 头部键名。
     * @param[in] def 默认值（键不存在或转换失败时返回）。
     * @return 转换后的值，若键不存在或转换失败则返回默认值。
     */
    template <class T>
    T getHeaderAs(const std::string& key, const T& def = T()) {
        return getAs(m_headers, key, def);
    }

    /**
     * @brief 获取所有头部。
     * @return 头部 Map。
     */
    const Map& getHeaders() const {
        return m_headers;
    }

    /**
     * @brief 设置所有头部。
     * @param[in] headers 头部 Map。
     */
    void setHeaders(const Map& headers) {
        m_headers = headers;
    }

    /**
     * @brief 获取所有 Cookie。
     * @return Cookie 字符串列表。
     */
    const std::vector<std::string>& getCookies() const {
        return m_cookies;
    }

    /**
     * @brief 设置所有 Cookie。
     * @param[in] cookies Cookie 字符串列表。
     */
    void setCookies(const std::vector<std::string>& cookies) {
        m_cookies = cookies;
    }

    /**
     * @brief 将响应转换为字符串。
     * @return 符合 HTTP 协议的响应字符串。
     */
    std::string toString() const;

    /**
     * @brief 输出响应到流。
     * @param[in,out] os 输出流。
     * @param[in] response HttpResponse 对象。
     * @return 输出流。
     */
    friend std::ostream& operator<<(std::ostream& os, const HttpResponse& response) {
        os << response.toString();
        return os;
    }

  private:
    HttpStatus               m_status;     ///< 响应状态码
    uint8_t                  m_version;    ///< HTTP 版本
    bool                     m_close;      ///< 是否关闭连接
    bool                     m_websocket;  ///< 是否为 WebSocket 响应
    std::string              m_body;       ///< 消息体
    std::string              m_reason;     ///< 原因短语
    Map                      m_headers;    ///< 头部 Map
    std::vector<std::string> m_cookies;    ///< Cookie 列表
};

};  // namespace zmuduo::net::http

#endif