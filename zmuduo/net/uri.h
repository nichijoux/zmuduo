// Original code - Copyright (c) sylar-yin —— sylar
// Modified by nichijoux

#ifndef ZMUDUO_NET_URI_H
#define ZMUDUO_NET_URI_H

#include "zmuduo/base/copyable.h"
#include "zmuduo/net/address.h"

#include <memory>
#include <string>

namespace zmuduo::net {
/**
 * @class Uri
 * @brief 表示和解析 URI（Uniform Resource Identifier）的类。
 *
 * Uri 类用于解析和操作 URI 字符串，遵循 RFC 3986 标准，支持 URI 的各个组成部分（scheme, userinfo,
 * host, port, path, query, fragment）。<br/> 提供静态方法创建 Uri
 * 实例、生成网络地址（Address）以及序列化为字符串。支持常见协议（如 http, https, ws, wss,
 * smtp）的默认端口判断。
 *
 * @note Uri 是可拷贝的（继承自 Copyable），通过智能指针（Ptr）管理实例。
 * @note URI 示例：
 * @code
 * foo://user@example.com:8042/over/there?name=ferret#nose
 * \_/   \___________________/\_________/ \_________/ \__/
 *  |                |             |           |        |
 * scheme          authority       path        query   fragment
 *
 * urn:example:animal:ferret:nose
 * @endcode
 *
 * @example
 *
 * @code
 * auto uri = Uri::Create("http://example.com:80/path?key=value#fragment");
 * if (uri) {
 *     std::cout << "Scheme: " << uri->getScheme() << std::endl;
 *     std::cout << "Host: " << uri->getHost() << std::endl;
 *     auto addr = uri->createAddress();
 *     if (addr) {
 *         std::cout << "Address: " << addr->toString() << std::endl;
 *     }
 * }
 * @endcode
 */
class Uri : public Copyable {
  public:
    /**
     * @typedef std::shared_ptr&lt;Uri&gt;
     * @brief Uri 智能指针类型。
     */
    using Ptr = std::shared_ptr<Uri>;

    /**
     * @brief 静态方法，创建 Uri 对象。
     * @param[in] uriStr URI 字符串。
     * @return 成功解析返回 Uri 智能指针，否则返回 nullptr。
     */
    static Uri::Ptr Create(const std::string& uriStr);

    /**
     * @brief 默认构造函数，初始化空 Uri 对象。
     * @note 端口号初始化为 0，其他成员为空字符串。
     */
    Uri() : m_port(0) {}

    /**
     * @brief 获取协议名（scheme）。
     * @return 协议名字符串（如 "http", "https"）。
     */
    const std::string& getScheme() const {
        return m_scheme;
    }

    /**
     * @brief 设置协议名（scheme）。
     * @param[in] v 协议名字符串。
     */
    void setScheme(const std::string& v) {
        m_scheme = v;
    }

    /**
     * @brief 获取用户信息（userinfo）。
     * @return 用户信息字符串（如 "user:password"）。
     */
    const std::string& getUserinfo() const {
        return m_userinfo;
    }

    /**
     * @brief 设置用户信息（userinfo）。
     * @param[in] v 用户信息字符串。
     */
    void setUserinfo(const std::string& v) {
        m_userinfo = v;
    }

    /**
     * @brief 获取主机名（host）。
     * @return 主机名字符串（如 "example.com"）。
     */
    const std::string& getHost() const {
        return m_host;
    }

    /**
     * @brief 设置主机名（host）。
     * @param[in] v 主机名字符串。
     */
    void setHost(const std::string& v) {
        m_host = v;
    }

    /**
     * @brief 获取路径（path）。
     * @return 路径字符串（如 "/over/there"）。
     */
    const std::string& getPath() const;

    /**
     * @brief 设置路径（path）。
     * @param[in] v 路径字符串。
     */
    void setPath(const std::string& v) {
        m_path = v;
    }

    /**
     * @brief 获取查询参数（query）。
     * @return 查询参数字符串（如 "name=ferret"）。
     */
    const std::string& getQuery() const {
        return m_query;
    }

    /**
     * @brief 设置查询参数（query）。
     * @param[in] v 查询参数字符串。
     */
    void setQuery(const std::string& v) {
        m_query = v;
    }

    /**
     * @brief 获取片段（fragment）。
     * @return 片段字符串（如 "nose"）。
     */
    const std::string& getFragment() const {
        return m_fragment;
    }

    /**
     * @brief 设置片段（fragment）。
     * @param[in] v 片段字符串。
     */
    void setFragment(const std::string& v) {
        m_fragment = v;
    }

    /**
     * @brief 获取端口号。
     * @return 端口号。
     */
    uint16_t getPort() const;

    /**
     * @brief 设置端口号。
     * @param[in] v 端口号。
     */
    void setPort(int32_t v) {
        m_port = v;
    }

    /**
     * @brief 将 Uri 序列化为字符串。
     * @return URI 字符串。
     */
    std::string toString() const;

    /**
     * @brief 根据 URI 创建网络地址。
     * @return 成功创建返回 Address 智能指针，否则返回 nullptr。
     */
    Address::Ptr createAddress() const;

    /**
     * @brief 输出 Uri 到流。
     * @param[in,out] os 输出流。
     * @param[in] uri Uri 对象。
     * @return 输出流。
     */
    friend std::ostream& operator<<(std::ostream& os, const Uri& uri) {
        os << uri.toString();
        return os;
    }

  private:
    /**
     * @brief 判断端口是否为协议的默认端口。
     * @retval true 端口是协议的默认端口（例如 http 的 80，https 的 443）。
     * @retval false 端口不是协议的默认端口。
     * @note 支持的协议包括 http, https, ws, wss, smtp。
     */
    bool isDefaultPort() const;

  private:
    std::string m_scheme;    ///< 协议名（如 "http", "https"）
    std::string m_userinfo;  ///< 用户信息（如 "user:password"）
    std::string m_host;      ///< 主机名（如 "example.com"）
    std::string m_path;      ///< 路径（如 "/over/there"）
    std::string m_query;     ///< 查询参数（如 "name=ferret"）
    std::string m_fragment;  ///< 片段（如 "nose"）
    uint16_t    m_port;      ///< 端口号
};

}  // namespace zmuduo::net

#endif