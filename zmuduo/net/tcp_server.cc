#include "zmuduo/net/tcp_server.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/event_loop_thread.h"
#include "zmuduo/net/socket_options.h"
#include <cassert>

#ifdef ZMUDUO_ENABLE_OPENSSL
#    include <openssl/err.h>
#endif

namespace zmuduo::net {
TcpServer::TcpServer(EventLoop*          loop,
                     const Address::Ptr& listenAddress,
                     const std::string&  name,
                     bool                reusePort /* = false */)
    : m_eventLoop(EventLoop::checkNotNull(loop)),
      m_ipPort(listenAddress->toString()),
      m_name(name),
      m_acceptor(std::make_unique<Acceptor>(loop, listenAddress, reusePort)),
      m_threadPool(std::make_shared<EventLoopThreadPool>(loop, name)),
      m_connectionCallback(defaultConnectionCallback),
      m_messageCallback(defaultMessageCallback) {
    // 设置acceptor的新连接回调
    m_acceptor->setNewConnectionCallback(
        [this](const int socketFD, const Address::Ptr& peerAddress) {
            newConnection(socketFD, peerAddress);
        });
}

TcpServer::~TcpServer() {
    m_eventLoop->assertInLoopThread();
    for (const auto& [name, connection] : m_connections) {
        connection->getEventLoop()->runInLoop([ptr = connection] { ptr->connectDestroyed(); });
    }
#ifdef ZMUDUO_ENABLE_OPENSSL
    if (m_sslContext) {
        SSL_CTX_free(m_sslContext);
    }
#endif
}

void TcpServer::setThreadNum(const int num) const {
    assert(num >= 0);
    m_threadPool->setThreadNum(num);
}

#ifdef ZMUDUO_ENABLE_OPENSSL
bool TcpServer::loadCertificates(const std::string& certificatePath,
                                 const std::string& privateKeyPath) {
    if (m_started || m_sslContext) {
        ZMUDUO_LOG_FMT_ERROR("tcpServer[%s] has started", m_name.c_str());
        return false;
    }
    ZMUDUO_LOG_INFO << "Try to set SSL";
    m_sslContext = SSL_CTX_new(SSLv23_server_method());
#    define CHECK_SSL_ERROR(expression, message, ...)                                              \
        do {                                                                                       \
            if (expression) {                                                                      \
                ZMUDUO_LOG_FMT_ERROR(message, ##__VA_ARGS__);                                      \
                SSL_CTX_free(m_sslContext);                                                        \
                m_sslContext = nullptr;                                                            \
                return false;                                                                      \
            }                                                                                      \
        } while (0)

    CHECK_SSL_ERROR(m_sslContext == nullptr, "err in SSL_CTX_new");

    // 加载服务端证书
    CHECK_SSL_ERROR(SSL_CTX_use_certificate_chain_file(m_sslContext, certificatePath.c_str()) <= 0,
                    "load %s error in SSL_CTX_use_certificate_chain_file", certificatePath.c_str());

    // 加载私钥
    CHECK_SSL_ERROR(
        SSL_CTX_use_PrivateKey_file(m_sslContext, privateKeyPath.c_str(), SSL_FILETYPE_PEM) <= 0,
        "load %s error in SSL_CTX_use_PrivateKey_file", privateKeyPath.c_str());

    // 检查证书和私钥是否匹配
    CHECK_SSL_ERROR(!SSL_CTX_check_private_key(m_sslContext), "error in SSL_CTX_check_private_key");

    // 设置验证选项
    SSL_CTX_set_verify(m_sslContext, SSL_VERIFY_PEER, nullptr);

#    undef CHECK_SSL_ERROR
    return true;
}
#endif

void TcpServer::start() {
    if (bool expected = false; m_started.compare_exchange_weak(expected, true)) {
        ZMUDUO_LOG_DEBUG << m_name << " started";
        // 启动服务器
        m_threadPool->start(m_threadInitCallback);
        // 监听accept的listen事件
        assert(!m_acceptor->isListening());
        m_eventLoop->runInLoop([acceptor = m_acceptor.get()] { acceptor->listen(); });
    }
}

void TcpServer::newConnection(int socketFD, const Address::Ptr& peerAddress) {
    m_eventLoop->assertInLoopThread();
    EventLoop* ioLoop = m_threadPool->getNextLoop();
    char       buffer[64];
    snprintf(buffer, sizeof(buffer), "-%s#%lu", m_ipPort.c_str(), m_nextConnectId++);
    std::string connectName = m_name + buffer;

    ZMUDUO_LOG_FMT_INFO("TcpServer[%s] - new connection [%s] from %s", m_name.c_str(),
                        connectName.c_str(), peerAddress->toString().c_str());
    // 获取本地的address
    const auto   addrin       = sockets::getLocalAddress(socketFD);
    Address::Ptr localAddress = Address::Create(sockets::sockaddr_cast(&addrin));
#ifdef ZMUDUO_ENABLE_OPENSSL
    // ssl
    SSL* ssl = nullptr;
    if (m_sslContext) {
        ssl = SSL_new(m_sslContext);
        SSL_set_fd(ssl, socketFD);
        SSL_set_accept_state(ssl);
    }
    // 创建tcp连接
    auto tcpConnection = std::make_shared<TcpConnection>(
        ioLoop, connectName, socketFD, localAddress, peerAddress, ssl);
#else
    // 创建TcpConnection并加入到连接中
    TcpConnectionPtr tcpConnection =
        std::make_shared<TcpConnection>(ioLoop, connectName, socketFD, localAddress, peerAddress);
#endif
    m_connections[connectName] = tcpConnection;
    // 设置tcp连接的回调函数
    // TcpServer => TcpConnection => Channel => Poller => notify Channel
    tcpConnection->setConnectionCallback(m_connectionCallback);
    tcpConnection->setMessageCallback(m_messageCallback);
    tcpConnection->setWriteCompleteCallback(m_writeCompleteCallback);
    // 设置连接关闭的回调函数，removeConnection会调用TcpConnection::connectDestroyed
    tcpConnection->setCloseCallback(
        [this](const TcpConnectionPtr& connection) { removeConnection(connection); });
    // 直接调用TcpConnection::connectEstablished通知连接完成，TcpConnection会启用读事件
    ioLoop->runInLoop([tcpConnection] { tcpConnection->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnectionPtr& connection) {
    m_eventLoop->runInLoop([this, connection] { removeConnectionInLoop(connection); });
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& connection) {
    m_eventLoop->assertInLoopThread();
    ZMUDUO_LOG_FMT_INFO("TcpServer[%s] - connection %s", m_name.c_str(),
                        connection->getName().c_str());
    // 从connections中移除name
    const size_t n = m_connections.erase(connection->getName());
    assert(n == 1);
    // 获取connection的ioLoop
    const auto ioLoop = connection->getEventLoop();
    // 调用TcpConnection::connectDestroyed
    ioLoop->queueInLoop([connection] { connection->connectDestroyed(); });
}
} // namespace zmuduo::net