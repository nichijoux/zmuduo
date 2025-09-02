#include "zmuduo/net/udp_server.h"
#include "buffer.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include <cassert>

namespace zmuduo::net {
UdpServer::UdpServer(EventLoop* loop, const Address::Ptr& listenAddress, const std::string& name)
    : m_eventLoop(EventLoop::checkNotNull(loop)),
      m_ipPort(listenAddress->toString()),
      m_name(name),
      m_socket(UdpSocket::Create(listenAddress->getFamily())),
      m_channel(loop, m_socket.getFD()),
      m_threadPool(std::make_shared<EventLoopThreadPool>(loop, name)),
      m_started(false) {
    ZMUDUO_LOG_FMT_DEBUG("%s[ctor-%p]", m_name.c_str(), this);
    // 绑定端口和设置重用端口
    m_socket.bind(listenAddress);
    m_socket.setReuseAddress(true);
    m_socket.setReusePort(true);
    // 设置回调
    m_channel.setReadCallback([this](auto) { handleRead(); });
}

UdpServer::~UdpServer() {
    m_eventLoop->assertInLoopThread();
    ZMUDUO_LOG_FMT_DEBUG("%s[dtor-%p]", m_name.c_str(), this);
    m_channel.disableAll();
    m_channel.remove();
}

void UdpServer::setThreadNum(const int num) const {
    assert(num >= 0);
    m_threadPool->setThreadNum(num);
}

void UdpServer::start() {
    if (bool expected = false; m_started.compare_exchange_weak(expected, true)) {
        ZMUDUO_LOG_FMT_DEBUG("[%s:%s] started", m_name.c_str(), m_ipPort.c_str());
        m_eventLoop->runInLoop([this] { m_channel.enableReading(); });
    }
}

void UdpServer::send(const void* data, size_t length, const Address::Ptr& peerAddress) const {
    EventLoop* ioLoop = m_threadPool->getNextLoop();
    ioLoop->runInLoop(
        [this, data, length, &peerAddress] { sendInLoop(data, length, peerAddress); });
}

void UdpServer::sendInLoop(const void*         data,
                           const size_t        length,
                           const Address::Ptr& peerAddress) const {
    // 发送
    ::sendto(m_socket.getFD(), data, length, 0, peerAddress->getSockAddress(),
             peerAddress->getSockAddressLength());
}

void UdpServer::handleRead() {
    EventLoop* ioLoop = m_threadPool->getNextLoop();
    ioLoop->runInLoop([this] { newMessage(); });
}

void UdpServer::newMessage() {
    Buffer        inputBuffer;
    sockaddr_in   sockaddrIn{};
    socklen_t     sockaddrLen = sizeof(sockaddrIn);
    const ssize_t n           = ::recvfrom(m_socket.getFD(), const_cast<char*>(inputBuffer.peek()),
                                 inputBuffer.getWriteableBytes(), 0,
                                 reinterpret_cast<sockaddr*>(&sockaddrIn), &sockaddrLen);
    if (n > 0) {
        inputBuffer.hasWritten(n);
        const auto peerAddress = Address::Create(reinterpret_cast<sockaddr*>(&sockaddrIn));
        ZMUDUO_LOG_FMT_INFO("[%s] - new message from %s", m_name.c_str(),
                            peerAddress->toString().c_str());
        if (m_messageCallback) {
            m_messageCallback(*this, inputBuffer, peerAddress);
        }
    } else {
        ZMUDUO_LOG_ERROR << "UdpServer recvFrom error: " << strerror(errno);
    }
}
} // namespace zmuduo::net