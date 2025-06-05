// Original code - Copyright (c) sylar-yin —— sylar
// Modified by nichijoux

#ifndef ZMUDUO_NET_ADDRESS_H
#define ZMUDUO_NET_ADDRESS_H

#include "zmuduo/net/endian.h"

#include <arpa/inet.h>
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <ostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>

namespace zmuduo::net {

class IPAddress;

/**
 * @brief 网络地址的基类,抽象类
 */
class Address {
  public:
    using Ptr = std::shared_ptr<Address>;

    /**
     * @brief 从 sockaddr 指针创建对应的 Address 对象
     * @param[in] addr 原始 sockaddr 指针
     * @return 创建成功则返回智能指针，失败返回 nullptr
     */
    static Address::Ptr Create(const sockaddr* addr);

    /**
     * @brief 通过主机名查询符合条件的所有地址信息
     * @param[out] result 存储返回的地址列表
     * @param[in] host 主机名/IP/域名（支持带端口的形式，如 example.com:80）
     * @param[in] family 协议族，如 AF_INET、AF_INET6、AF_UNIX
     * @param[in] type 套接字类型，如 SOCK_STREAM、SOCK_DGRAM
     * @param[in] protocol 协议类型，如 IPPROTO_TCP、IPPROTO_UDP
     * @retval true 查询成功
     * @retval false 查询失败
     */
    static bool Lookup(std::vector<Address::Ptr>& result,
                       const std::string&         host,
                       int                        family   = AF_INET,
                       int                        type     = 0,
                       int                        protocol = 0);

    /**
     * @brief 查询任意符合条件的地址
     * @return 若成功则返回一个匹配的地址，否则返回 nullptr
     */
    static Address::Ptr
    LookupAny(const std::string& host, int family = AF_INET, int type = 0, int protocol = 0);

    /**
     * @brief 查询任意符合条件的 IPAddress 地址
     * @return 若成功则返回 IPAddress 智能指针，否则返回 nullptr
     */
    static std::shared_ptr<IPAddress> LookupAnyIPAddress(const std::string& host,
                                                         int                family   = AF_INET,
                                                         int                type     = 0,
                                                         int                protocol = 0);

    /**
     * @brief 获取本地所有网卡的地址信息
     * @param[out] result 保存网卡名与对应的地址、掩码位
     * @param[in] family 协议族（默认 IPv4）
     * @retval true 获取成功
     * @retval false 获取失败
     */
    static bool
    GetInterfaceAddresses(std::multimap<std::string, std::pair<Address::Ptr, uint32_t>>& result,
                          int family = AF_INET);

    /**
     * @brief 获取指定网卡的地址信息
     * @param[out] result 保存该网卡的地址和子网掩码位数
     * @param[in] iface 网卡名称
     * @retval true 获取成功
     * @retval false 获取失败
     */
    static bool GetInterfaceAddresses(std::vector<std::pair<Address::Ptr, uint32_t>>& result,
                                      const std::string&                              iface,
                                      int family = AF_INET);

    /**
     * @brief 虚析构函数
     */
    virtual ~Address() = default;

    /**
     * @brief 返回协议簇
     */
    int getFamily() const;

    /**
     * @brief 返回sockaddr指针,只读
     */
    virtual const sockaddr* getSockAddress() const = 0;

    /**
     * @brief 返回sockaddr指针,读写
     */
    virtual sockaddr* getSockAddress() = 0;

    /**
     * @brief 返回sockaddr的长度
     */
    virtual socklen_t getSockAddressLength() const = 0;

    /**
     * @brief 可读性输出地址
     */
    virtual std::ostream& dump(std::ostream& os) const = 0;

    /**
     * @brief 返回可读性字符串
     */
    std::string toString() const;

    /**
     * @brief 小于号比较函数
     */
    bool operator<(const Address& rhs) const;

    /**
     * @brief 等于函数
     */
    bool operator==(const Address& rhs) const;

    /**
     * @brief 不等于函数
     */
    bool operator!=(const Address& rhs) const;

    /**
     * @brief 流式输出Address
     */
    friend std::ostream& operator<<(std::ostream& os, const Address& addr) {
        addr.dump(os);
        return os;
    }
};

/**
 * @class IPAddress
 * @brief 表示IP地址的抽象基类，继承自Address。
 */
class IPAddress : public Address {
  public:
    using Ptr = std::shared_ptr<IPAddress>;

    /**
     * @brief 通过域名,IP,服务器名创建IPAddress
     * @param[in] address 域名,IP,服务器名等.举例: www.sylar.top
     * @param[in] port 端口号
     * @return 调用成功返回IPAddress,失败返回nullptr
     */
    static IPAddress::Ptr Create(const char* address, uint16_t port = 0);

    /**
     * @brief 获取该地址对应的广播地址。
     * @param[in] length 子网掩码位数
     * @return 返回对应的广播地址实例
     */
    virtual IPAddress::Ptr getBroadcastAddress(uint32_t length) = 0;

    /**
     * @brief 获取该地址对应的网络地址。
     * @param[in] length 子网掩码位数
     * @return 返回对应的网络地址实例
     */
    virtual IPAddress::Ptr getNetworkAddress(uint32_t length) = 0;

    /**
     * @brief 获取该地址对应的子网掩码地址。
     * @param[in] length 子网掩码位数
     * @return 返回对应的子网掩码地址实例
     */
    virtual IPAddress::Ptr getSubnetMask(uint32_t length) = 0;

    /**
     * @brief 获取端口号
     * @return 当前地址的端口号
     */
    virtual uint32_t getPort() const = 0;

    /**
     * @brief 设置端口号
     * @param[in] v 新的端口号
     */
    virtual void setPort(uint16_t v) = 0;
};

/**
 * @class IPv4Address
 * @brief IPv4地址类，继承自IPAddress。
 */
class IPv4Address : public IPAddress {
  public:
    using Ptr = std::shared_ptr<IPv4Address>;

    /**
     * @brief 使用点分十进制地址创建IPv4Address
     * @param[in] address 点分十进制地址,如:192.168.1.1
     * @param[in] port 端口号
     * @return 返回IPv4Address,失败返回nullptr
     */
    static IPv4Address::Ptr Create(const char* address = "127.0.0.1", uint16_t port = 0);

    /**
     * @brief 使用sockaddr_in结构体构造IPv4地址
     * @param[in] address IPv4格式的sockaddr结构体
     */
    explicit IPv4Address(const sockaddr_in& address) : m_addr(address) {}

    /**
     * @brief 使用原始IP和端口构造IPv4地址
     * @param[in] address 32位二进制IP地址
     * @param[in] port 端口号
     */
    explicit IPv4Address(uint32_t address = INADDR_ANY, uint16_t port = 0);

    const sockaddr* getSockAddress() const override;
    sockaddr*       getSockAddress() override;

    socklen_t getSockAddressLength() const override;

    IPAddress::Ptr getBroadcastAddress(uint32_t length) override;
    IPAddress::Ptr getNetworkAddress(uint32_t length) override;
    IPAddress::Ptr getSubnetMask(uint32_t prefix_len) override;

    uint32_t getPort() const override;
    void     setPort(uint16_t v) override;

    std::ostream& dump(std::ostream& os) const override;

  private:
    sockaddr_in m_addr;
};

/**
 * @class IPv6Address
 * @brief IPv6地址类，继承自IPAddress。
 */
class IPv6Address : public IPAddress {
  public:
    typedef std::shared_ptr<IPv6Address> Ptr;
    /**
     * @brief 通过IPv6地址字符串构造IPv6Address
     * @param[in] address IPv6地址字符串
     * @param[in] port 端口号
     */
    static IPv6Address::Ptr Create(const char* address, uint16_t port = 0);

    /**
     * @brief 默认构造函数，初始化为::1（loopback）地址
     */
    IPv6Address();

    /**
     * @brief 通过sockaddr_in6结构体构造IPv6地址
     * @param[in] address sockaddr_in6结构体
     */
    explicit IPv6Address(const sockaddr_in6& address);

    /**
     * @brief 通过IPv6二进制地址和端口构造IPv6地址
     * @param[in] address 16字节IPv6地址
     * @param[in] port 端口号
     */
    explicit IPv6Address(const uint8_t address[16], uint16_t port = 0);

    const sockaddr* getSockAddress() const override;
    sockaddr*       getSockAddress() override;
    socklen_t       getSockAddressLength() const override;
    std::ostream&   dump(std::ostream& os) const override;

    IPAddress::Ptr getBroadcastAddress(uint32_t length) override;
    IPAddress::Ptr getNetworkAddress(uint32_t length) override;
    IPAddress::Ptr getSubnetMask(uint32_t prefix_len) override;
    uint32_t       getPort() const override;
    void           setPort(uint16_t v) override;

  private:
    sockaddr_in6 m_addr;  ///< IPv6地址结构体
};

/**
 * @brief Unix 本地套接字地址类，继承自Address。
 */
class UnixAddress : public Address {
  public:
    using Ptr = std::shared_ptr<UnixAddress>;

    /**
     * @brief 默认构造函数，初始化空地址
     */
    UnixAddress();

    /**
     * @brief 使用路径构造UnixAddress
     * @param[in] path 文件系统路径，表示Unix域套接字地址
     */
    explicit UnixAddress(const std::string& path);

    const sockaddr* getSockAddress() const override;
    sockaddr*       getSockAddress() override;
    socklen_t       getSockAddressLength() const override;
    /**
     * @brief 设置地址长度
     * @param[in] v 地址结构体长度
     */
    void setSockAddressLength(uint32_t v);

    /**
     * @brief 获取 Unix 域套接字地址的路径字符串。
     *
     * @note 对于抽象命名空间地址（sun_path[0] == '\0'），
     *       返回的字符串将以 "\\0" 开头，以区分普通路径。
     *
     * @return 路径字符串或抽象命名空间名（带 \0 前缀显示）。
     */
    std::string getPath() const;

    std::ostream& dump(std::ostream& os) const override;

  private:
    sockaddr_un m_addr;    ///< Unix 套接字地址结构体
    socklen_t   m_length;  ///< 地址长度
};

/**
 * @class UnknownAddress
 * @brief 未知类型地址，通常用于未识别协议族。
 */
class UnknownAddress : public Address {
  public:
    using Ptr = std::shared_ptr<UnknownAddress>;

    /**
     * @brief 使用协议族构造未知地址
     * @param[in] family 协议族编号
     */
    explicit UnknownAddress(int family);

    /**
     * @brief 使用sockaddr构造未知地址
     * @param[in] addr 未知类型的原始地址结构体
     */
    explicit UnknownAddress(const sockaddr& addr) : m_addr(addr) {}

    const sockaddr* getSockAddress() const override;
    sockaddr*       getSockAddress() override;
    socklen_t       getSockAddressLength() const override;

    std::ostream& dump(std::ostream& os) const override;

  private:
    sockaddr m_addr;  ///< 原始地址结构体
};

}  // namespace zmuduo::net

#endif