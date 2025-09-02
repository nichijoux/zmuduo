#include "zmuduo/net/connector.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/socket.h"
#include "zmuduo/net/socket_options.h"
#include <cassert>
#include <memory>
#include <unistd.h>
#include <utility>

namespace zmuduo::net {
Connector::Connector(EventLoop* loop, Address::Ptr serverAddress)
    : m_eventLoop(loop),
      m_serverAddress(std::move(serverAddress)) {
    ZMUDUO_LOG_FMT_DEBUG("ctor[%p]", this);
}

Connector::~Connector() {
    ZMUDUO_LOG_FMT_DEBUG("dtor[%p]", this);
}

void Connector::start() {
    m_connect = true;
    m_eventLoop->runInLoop([this] { startInLoop(); });
}

void Connector::restart() {
    m_state        = State::DISCONNECTED;
    m_retryDelayMs = S_INIT_RETRY_DELAY_MS;
    start();
}

void Connector::disconnect() {
    m_connect = false;
    m_state   = State::DISCONNECTED;
}

void Connector::stop() {
    m_connect = false;
    m_eventLoop->runInLoop([this] { stopInLoop(); });
}

void Connector::startInLoop() {
    m_eventLoop->assertInLoopThread();
    assert(m_state == State::DISCONNECTED);
    if (m_connect) {
        connect();
    } else {
        ZMUDUO_LOG_FMT_DEBUG("do not connect");
    }
}

void Connector::stopInLoop() {
    m_eventLoop->assertInLoopThread();
    if (m_state == State::CONNECTING) {
        m_state            = State::DISCONNECTED;
        const int socketFD = removeAndResetChannel();
        retry(socketFD);
    }
}

void Connector::connect() {
    const int socketFD = sockets::createNonblockingOrDie(m_serverAddress->getFamily());
    // 非阻塞模式下，connect 会立即返回 EINPROGRESS（连接正在进行中）
    const int ret = ::connect(socketFD, m_serverAddress->getSockAddress(),
                              m_serverAddress->getSockAddressLength());
    switch (const int savedErrno = ret == 0 ? 0 : errno) {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN: connecting(socketFD);
            break;

        case EAGAIN:
        case EADDRINUSE:
        case EADDRNOTAVAIL:
        case ECONNREFUSED:
        case ENETUNREACH: retry(socketFD);
            break;

        case EACCES:
        case EPERM:
        case EAFNOSUPPORT:
        case EALREADY:
        case EBADF:
        case EFAULT:
        case ENOTSOCK:
            ZMUDUO_LOG_FMT_ERROR("connect error in Connector::startInLoop %d", savedErrno);
            ::close(socketFD);
            break;

        default:
            ZMUDUO_LOG_FMT_ERROR("Unexpected error in Connector::startInLoop %d", savedErrno);
            ::close(socketFD);
            break;
    }
}

void Connector::connecting(int socketFD) {
    m_state = State::CONNECTING;
    assert(!m_channel);
    m_channel = std::make_unique<Channel>(m_eventLoop, socketFD);
    // 当非阻塞 connect() 完成三次握手后，操作系统会将套接字标记为 可写
    // 若连接失败（如目标拒绝、超时、网络不可达），套接字同样会变为可写，
    // 但通过getsockopt(SO_ERROR) 可以获取错误码
    m_channel->setWriteCallback([ptr = shared_from_this()] { ptr->handleWrite(); });
    m_channel->setErrorCallback([ptr = shared_from_this()] { ptr->handleError(); });
    m_channel->enableWriting();
}

void Connector::handleWrite() {
    // 没有连接成功
    if (m_state == State::CONNECTING) {
        const int socketFD = removeAndResetChannel();
        if (const int err = sockets::getSocketError(socketFD)) {
            ZMUDUO_LOG_FMT_ERROR("SO_ERROR = %s", strerror(err));
            retry(socketFD);
        } else if (sockets::isSelfConnect(socketFD)) {
            ZMUDUO_LOG_FMT_WARNING("Self connect");
            retry(socketFD);
        } else {
            m_state = State::CONNECTED;
            if (m_connect) {
                m_newConnectionCallback(socketFD);
            } else {
                ::close(socketFD);
            }
        }
    }
}

void Connector::handleError() {
    ZMUDUO_LOG_FMT_ERROR("state %d", static_cast<int>(m_state));
    if (m_state == State::CONNECTING) {
        const int socketFD = removeAndResetChannel();
        const int err      = sockets::getSocketError(socketFD);
        ZMUDUO_LOG_FMT_ERROR("SO_ERROR = %s", strerror(err));
        retry(socketFD);
    }
}

void Connector::retry(const int socketFD) {
    ::close(socketFD);
    m_state = State::DISCONNECTED;
    if (m_connect) {
        // delay后再尝试重连
        ZMUDUO_LOG_FMT_INFO("Connector::retry - Delaying %dms before new connection",
                            m_retryDelayMs);
        m_eventLoop->runAfter(m_retryDelayMs / 1000.0,
                              [ptr = shared_from_this()] { ptr->startInLoop(); });
        m_retryDelayMs = std::min(m_retryDelayMs * 2, S_MAX_RETRY_DELAY_MS);
    } else {
        ZMUDUO_LOG_FMT_DEBUG("do not connect");
    }
}

int Connector::removeAndResetChannel() {
    // 清理临时 Channel
    m_channel->disableAll();
    m_channel->remove();
    const int socketFD = m_channel->getFD();
    m_eventLoop->queueInLoop([ptr = shared_from_this()] { ptr->resetChannel(); });
    return socketFD;
}

void Connector::resetChannel() {
    m_channel.reset();
}
} // namespace zmuduo::net