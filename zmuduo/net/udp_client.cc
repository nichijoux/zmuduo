#include "zmuduo/net/udp_client.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"

#include <utility>

namespace zmuduo::net {
UdpClient::UdpClient(EventLoop* loop, Address::Ptr serverAddress, int domain, std::string name)
    : m_eventLoop(loop),
      m_serverAddress(std::move(serverAddress)),
      m_name(std::move(name)),
      m_socket(UdpSocket::Create(domain)),
      m_channel(std::make_unique<Channel>(loop, m_socket.getFD())) {
    ZMUDUO_LOG_FMT_INFO("UdpClient[%s] ctor", m_name.c_str());
    m_channel->setReadCallback([this](auto) { handleRead(); });
}

UdpClient::~UdpClient() {
    m_channel->disableAll();
    m_channel->remove();
}

void UdpClient::start() const {
    m_channel->enableReading();
}

void UdpClient::stop() const {
    m_channel->disableAll();
}

void UdpClient::send(const void* data, size_t length) const {
    m_eventLoop->runInLoop([this, data, length]() { sendInLoop(data, length); });
}

void UdpClient::sendInLoop(const void* message, const size_t length) const {
    ::sendto(m_socket.getFD(), message, length, 0, m_serverAddress->getSockAddress(),
             m_serverAddress->getSockAddressLength());
}

void UdpClient::handleRead() {
    sockaddr_in   sockaddrIn{};
    socklen_t     sockaddrLen = sizeof(sockaddrIn);
    const ssize_t n = ::recvfrom(m_socket.getFD(), const_cast<char*>(m_inputBuffer.peek()),
                                 m_inputBuffer.getWriteableBytes(), 0,
                                 reinterpret_cast<sockaddr*>(&sockaddrIn), &sockaddrLen);
    if (n > 0) {
        // 标记字节已经写入
        m_inputBuffer.hasWritten(n);
        if (memcmp(&sockaddrIn, m_serverAddress->getSockAddress(),
                   m_serverAddress->getSockAddressLength()) != 0) {
            // 如果不是服务器地址,则打印错误信息
            char        ipStr[INET_ADDRSTRLEN] = {0};
            const char* peerIp = inet_ntop(AF_INET, &sockaddrIn.sin_addr, ipStr, INET_ADDRSTRLEN);
            ZMUDUO_LOG_FMT_ERROR("[%s] - new message from %s, not expect server address",
                                 m_name.c_str(), peerIp);
        } else {
            // 消息回调
            if (m_messageCallback) {
                m_messageCallback(*this, m_inputBuffer);
            }
        }
    } else {
        ZMUDUO_LOG_ERROR << "UdpClient recvFrom error: " << strerror(errno);
    }
}
} // namespace zmuduo::net