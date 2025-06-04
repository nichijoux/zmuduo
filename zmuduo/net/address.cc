#include "zmuduo/net/address.h"
#include "zmuduo/base/logger.h"
#include <cstddef>
#include <ifaddrs.h>
#include <memory>
#include <netdb.h>
#include <sstream>

#include "endian.h"

namespace zmuduo::net {

template <class T>
static T CreateMask(uint32_t bits) {
    return (1 << (sizeof(T) * 8 - bits)) - 1;
}

template <class T>
static uint32_t CountBytes(T value) {
    uint32_t result = 0;
    for (; value; ++result) {
        value &= value - 1;
    }
    return result;
}

Address::Ptr Address::LookupAny(const std::string& host, int family, int type, int protocol) {
    std::vector<Address::Ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        return result[0];
    }
    return nullptr;
}

IPAddress::Ptr
Address::LookupAnyIPAddress(const std::string& host, int family, int type, int protocol) {
    std::vector<Address::Ptr> result;
    if (Lookup(result, host, family, type, protocol)) {
        for (auto& i : result) {
            // 尝试转为IP地址
            IPAddress::Ptr v = std::dynamic_pointer_cast<IPAddress>(i);
            if (v) {
                return v;
            }
        }
    }
    return nullptr;
}

bool Address::Lookup(std::vector<Address::Ptr>& result,
                     const std::string&         host,
                     int                        family /* = AF_INET */,
                     int                        type /* = 0 */,
                     int                        protocol /* = 0 */) {
    addrinfo hints{}, *results, *next;
    hints.ai_flags     = 0;
    hints.ai_family    = family;
    hints.ai_socktype  = type;
    hints.ai_protocol  = protocol;
    hints.ai_addrlen   = 0;
    hints.ai_canonname = nullptr;
    hints.ai_addr      = nullptr;
    hints.ai_next      = nullptr;

    std::string node;
    const char* service = nullptr;

    // 检查 ipv6address serivce
    if (!host.empty() && host[0] == '[') {
        const char* endipv6 = (const char*)memchr(host.c_str() + 1, ']', host.size() - 1);
        if (endipv6) {
            // TODO check out of range
            if (*(endipv6 + 1) == ':') {
                service = endipv6 + 2;
            }
            node = host.substr(1, endipv6 - host.c_str() - 1);
        }
    }

    // 检查 node serivce
    if (node.empty()) {
        service = (const char*)memchr(host.c_str(), ':', host.size());
        if (service) {
            if (!memchr(service + 1, ':', host.c_str() + host.size() - service - 1)) {
                node = host.substr(0, service - host.c_str());
                ++service;
            }
        }
    }

    if (node.empty()) {
        node = host;
    }
    int error = getaddrinfo(node.c_str(), service, &hints, &results);
    if (error) {
        ZMUDUO_LOG_DEBUG << "Address::Lookup getaddress(" << host << ", " << family << ", " << type
                         << ") err=" << error << " errstr=" << gai_strerror(error);
        return false;
    }

    next = results;
    while (next) {
        result.emplace_back(Create(next->ai_addr));
        next = next->ai_next;
    }

    freeaddrinfo(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(
    std::multimap<std::string, std::pair<Address::Ptr, uint32_t>>& result,
    int                                                            family) {
    struct ifaddrs *next, *results;
    if (getifaddrs(&results) != 0) {
        ZMUDUO_LOG_DEBUG << "Address::GetInterfaceAddresses getifaddrs "
                            " err="
                         << errno << " errstr=" << strerror(errno);
        return false;
    }

    try {
        for (next = results; next; next = next->ifa_next) {
            Address::Ptr addr;
            uint32_t     prefix_len = ~0u;
            if (family != AF_UNSPEC && family != next->ifa_addr->sa_family) {
                continue;
            }
            switch (next->ifa_addr->sa_family) {
                case AF_INET: {
                    addr             = Create(next->ifa_addr);
                    uint32_t netmask = ((sockaddr_in*)next->ifa_netmask)->sin_addr.s_addr;
                    prefix_len       = CountBytes(netmask);
                } break;
                case AF_INET6: {
                    addr              = Create(next->ifa_addr);
                    in6_addr& netmask = ((sockaddr_in6*)next->ifa_netmask)->sin6_addr;
                    prefix_len        = 0;
                    for (int i = 0; i < 16; ++i) {
                        prefix_len += CountBytes(netmask.s6_addr[i]);
                    }
                } break;
                default: break;
            }

            if (addr) {
                result.insert(std::make_pair(next->ifa_name, std::make_pair(addr, prefix_len)));
            }
        }
    } catch (...) {
        ZMUDUO_LOG_ERROR << "Address::GetInterfaceAddresses exception";
        freeifaddrs(results);
        return false;
    }
    freeifaddrs(results);
    return !result.empty();
}

bool Address::GetInterfaceAddresses(std::vector<std::pair<Address::Ptr, uint32_t>>& result,
                                    const std::string&                              iface,
                                    int                                             family) {
    if (iface.empty() || iface == "*") {
        if (family == AF_INET || family == AF_UNSPEC) {
            result.emplace_back(std::make_shared<IPv4Address>(), 0u);
        }
        if (family == AF_INET6 || family == AF_UNSPEC) {
            result.emplace_back(std::make_shared<IPv6Address>(), 0u);
        }
        return true;
    }

    std::multimap<std::string, std::pair<Address::Ptr, uint32_t>> results;

    if (!GetInterfaceAddresses(results, family)) {
        return false;
    }

    auto its = results.equal_range(iface);
    for (; its.first != its.second; ++its.first) {
        result.push_back(its.first->second);
    }
    return !result.empty();
}

int Address::getFamily() const {
    return getSockAddress()->sa_family;
}

std::string Address::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

Address::Ptr Address::Create(const sockaddr* addr) {
    if (addr == nullptr) {
        return nullptr;
    }

    Address::Ptr result;
    switch (addr->sa_family) {
        case AF_INET: result = std::make_shared<IPv4Address>(*(const sockaddr_in*)addr); break;
        case AF_INET6: result = std::make_shared<IPv6Address>(*(const sockaddr_in6*)addr); break;
        default: result = std::make_shared<UnknownAddress>(*addr); break;
    }
    return result;
}

bool Address::operator<(const Address& rhs) const {
    socklen_t minlen = std::min(getSockAddressLength(), rhs.getSockAddressLength());
    int       result = memcmp(getSockAddress(), rhs.getSockAddress(), minlen);
    if (result > 0) {
        return false;
    } else {
        return result < 0 || getSockAddressLength() < rhs.getSockAddressLength();
    }
}

bool Address::operator==(const Address& rhs) const {
    return getSockAddressLength() == rhs.getSockAddressLength() &&
           memcmp(getSockAddress(), rhs.getSockAddress(), getSockAddressLength()) == 0;
}

bool Address::operator!=(const Address& rhs) const {
    return !(*this == rhs);
}

IPAddress::Ptr IPAddress::Create(const char* address, uint16_t port) {
    addrinfo hints{}, *results;
    bzero(&hints, sizeof(addrinfo));

    hints.ai_flags  = AI_NUMERICHOST;
    hints.ai_family = AF_UNSPEC;

    int error = getaddrinfo(address, nullptr, &hints, &results);
    if (error) {
        ZMUDUO_LOG_DEBUG << "IPAddress::Create(" << address << ", " << port << ") error=" << error
                         << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }

    try {
        IPAddress::Ptr result =
            std::dynamic_pointer_cast<IPAddress>(Address::Create(results->ai_addr));
        if (result) {
            result->setPort(port);
        }
        freeaddrinfo(results);
        return result;
    } catch (...) {
        freeaddrinfo(results);
        return nullptr;
    }
}

IPv4Address::Ptr IPv4Address::Create(const char* address, uint16_t port) {
    IPv4Address::Ptr rt = std::make_shared<IPv4Address>();
    rt->m_addr.sin_port = byteSwapOnLittleEndian(port);
    int result          = inet_pton(AF_INET, address, &rt->m_addr.sin_addr);
    if (result <= 0) {
        ZMUDUO_LOG_DEBUG << "IPv4Address::Create(" << address << ", " << port << ") rt=" << result
                         << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv4Address::IPv4Address(uint32_t address, uint16_t port) : m_addr() {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sin_family      = AF_INET;
    m_addr.sin_port        = byteSwapOnLittleEndian(port);
    m_addr.sin_addr.s_addr = byteSwapOnLittleEndian(address);
}

sockaddr* IPv4Address::getSockAddress() {
    return (sockaddr*)&m_addr;
}

const sockaddr* IPv4Address::getSockAddress() const {
    return (sockaddr*)&m_addr;
}

socklen_t IPv4Address::getSockAddressLength() const {
    return sizeof(m_addr);
}

std::ostream& IPv4Address::dump(std::ostream& os) const {
    uint32_t addr = byteSwapOnLittleEndian(m_addr.sin_addr.s_addr);
    os << ((addr >> 24) & 0xff) << "." << ((addr >> 16) & 0xff) << "." << ((addr >> 8) & 0xff)
       << "." << (addr & 0xff);
    os << ":" << byteSwapOnLittleEndian(m_addr.sin_port);
    return os;
}

IPAddress::Ptr IPv4Address::getBroadcastAddress(uint32_t length) {
    if (length > 32) {
        return nullptr;
    }

    sockaddr_in address(m_addr);
    address.sin_addr.s_addr |= byteSwapOnLittleEndian(CreateMask<uint32_t>(length));
    return std::make_shared<IPv4Address>(address);
}

IPAddress::Ptr IPv4Address::getNetworkAddress(uint32_t length) {
    if (length > 32) {
        return nullptr;
    }

    sockaddr_in address(m_addr);
    address.sin_addr.s_addr &= byteSwapOnLittleEndian(CreateMask<uint32_t>(length));
    return std::make_shared<IPv4Address>(address);
}

IPAddress::Ptr IPv4Address::getSubnetMask(uint32_t prefix_len) {
    sockaddr_in subnet{};
    bzero(&subnet, sizeof(subnet));
    subnet.sin_family      = AF_INET;
    subnet.sin_addr.s_addr = ~byteSwapOnLittleEndian(CreateMask<uint32_t>(prefix_len));
    return std::make_shared<IPv4Address>(subnet);
}

uint32_t IPv4Address::getPort() const {
    return byteSwapOnLittleEndian(m_addr.sin_port);
}

void IPv4Address::setPort(uint16_t v) {
    m_addr.sin_port = byteSwapOnLittleEndian(v);
}

IPv6Address::Ptr IPv6Address::Create(const char* address, uint16_t port) {
    IPv6Address::Ptr rt(new IPv6Address);
    rt->m_addr.sin6_port = byteSwapOnLittleEndian(port);
    int result           = inet_pton(AF_INET6, address, &rt->m_addr.sin6_addr);
    if (result <= 0) {
        ZMUDUO_LOG_DEBUG << "IPv6Address::Create(" << address << ", " << port << ") rt=" << result
                         << " errno=" << errno << " errstr=" << strerror(errno);
        return nullptr;
    }
    return rt;
}

IPv6Address::IPv6Address() : m_addr() {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
}

IPv6Address::IPv6Address(const sockaddr_in6& address) : m_addr(address) {}

IPv6Address::IPv6Address(const uint8_t address[16], uint16_t port) : m_addr() {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port   = byteSwapOnLittleEndian(port);
    memcpy(&m_addr.sin6_addr.s6_addr, address, 16);
}

sockaddr* IPv6Address::getSockAddress() {
    return (sockaddr*)&m_addr;
}

const sockaddr* IPv6Address::getSockAddress() const {
    return (sockaddr*)&m_addr;
}

socklen_t IPv6Address::getSockAddressLength() const {
    return sizeof(m_addr);
}

std::ostream& IPv6Address::dump(std::ostream& os) const {
    os << "[";
    auto* addr       = (uint16_t*)m_addr.sin6_addr.s6_addr;
    bool  used_zeros = false;
    for (size_t i = 0; i < 8; ++i) {
        if (addr[i] == 0 && !used_zeros) {
            continue;
        }
        if (i && addr[i - 1] == 0 && !used_zeros) {
            os << ":";
            used_zeros = true;
        }
        if (i) {
            os << ":";
        }
        os << std::hex << (int)byteSwapOnLittleEndian(addr[i]) << std::dec;
    }

    if (!used_zeros && addr[7] == 0) {
        os << "::";
    }

    os << "]:" << byteSwapOnLittleEndian(m_addr.sin6_port);
    return os;
}

IPAddress::Ptr IPv6Address::getBroadcastAddress(uint32_t length) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[length / 8] |= CreateMask<uint8_t>(length % 8);
    for (int i = static_cast<int>(length / 8) + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0xff;
    }
    return std::make_shared<IPv6Address>(baddr);
}

IPAddress::Ptr IPv6Address::getNetworkAddress(uint32_t length) {
    sockaddr_in6 baddr(m_addr);
    baddr.sin6_addr.s6_addr[length / 8] &= CreateMask<uint8_t>(length % 8);
    for (int i = static_cast<int>(length / 8) + 1; i < 16; ++i) {
        baddr.sin6_addr.s6_addr[i] = 0x00;
    }
    return std::make_shared<IPv6Address>(baddr);
}

IPAddress::Ptr IPv6Address::getSubnetMask(uint32_t prefix_len) {
    sockaddr_in6 subnet{};
    bzero(&subnet, sizeof(subnet));
    subnet.sin6_family                       = AF_INET6;
    subnet.sin6_addr.s6_addr[prefix_len / 8] = ~CreateMask<uint8_t>(prefix_len % 8);

    for (uint32_t i = 0; i < prefix_len / 8; ++i) {
        subnet.sin6_addr.s6_addr[i] = 0xff;
    }
    return std::make_shared<IPv6Address>(subnet);
}

uint32_t IPv6Address::getPort() const {
    return byteSwapOnLittleEndian(m_addr.sin6_port);
}

void IPv6Address::setPort(uint16_t v) {
    m_addr.sin6_port = byteSwapOnLittleEndian(v);
}

static const size_t MAX_PATH_LEN = sizeof(((sockaddr_un*)nullptr)->sun_path) - 1;

UnixAddress::UnixAddress() : m_addr(), m_length(0) {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length          = offsetof(sockaddr_un, sun_path) + MAX_PATH_LEN;
}

UnixAddress::UnixAddress(const std::string& path) : m_addr(), m_length(0) {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sun_family = AF_UNIX;
    m_length          = path.size() + 1;

    if (!path.empty() && path[0] == '\0') {
        --m_length;
    }

    if (m_length > sizeof(m_addr.sun_path)) {
        throw std::logic_error("path too long");
    }
    memcpy(m_addr.sun_path, path.c_str(), m_length);
    m_length += offsetof(sockaddr_un, sun_path);
}

void UnixAddress::setSockAddressLength(uint32_t v) {
    m_length = v;
}

sockaddr* UnixAddress::getSockAddress() {
    return (sockaddr*)&m_addr;
}

const sockaddr* UnixAddress::getSockAddress() const {
    return (sockaddr*)&m_addr;
}

socklen_t UnixAddress::getSockAddressLength() const {
    return m_length;
}

std::string UnixAddress::getPath() const {
    std::stringstream ss;
    if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        ss << "\\0"
           << std::string(m_addr.sun_path + 1, m_length - offsetof(sockaddr_un, sun_path) - 1);
    } else {
        ss << m_addr.sun_path;
    }
    return ss.str();
}

std::ostream& UnixAddress::dump(std::ostream& os) const {
    if (m_length > offsetof(sockaddr_un, sun_path) && m_addr.sun_path[0] == '\0') {
        return os << "\\0"
                  << std::string(m_addr.sun_path + 1,
                                 m_length - offsetof(sockaddr_un, sun_path) - 1);
    }
    return os << m_addr.sun_path;
}

UnknownAddress::UnknownAddress(int family) : m_addr() {
    bzero(&m_addr, sizeof(m_addr));
    m_addr.sa_family = family;
}

sockaddr* UnknownAddress::getSockAddress() {
    return (sockaddr*)&m_addr;
}

const sockaddr* UnknownAddress::getSockAddress() const {
    return &m_addr;
}

socklen_t UnknownAddress::getSockAddressLength() const {
    return sizeof(m_addr);
}

std::ostream& UnknownAddress::dump(std::ostream& os) const {
    os << "[UnknownAddress family=" << m_addr.sa_family << "]";
    return os;
}
}  // namespace zmuduo::net
