#include "zmuduo/net/tcp_client.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/net/connector.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/socket_options.h"
#include <cassert>
#include <utility>

#ifdef ZMUDUO_ENABLE_OPENSSL
#    include <openssl/err.h>
#endif

namespace zmuduo::net {

std::atomic<int64_t> TcpClient::S_NEXT_ID(0);

TcpClient::TcpClient(EventLoop* loop, const Address::Ptr& serverAddress, std::string name)
    : m_eventLoop(loop),
      m_connector(std::make_shared<Connector>(loop, serverAddress)),
      m_name(std::move(name)),
      m_connectionCallback(defaultConnectionCallback),
      m_messageCallback(defaultMessageCallback),
      m_retry(false),
      m_connected(false),
      m_mutex(),
      m_connection(nullptr)
#ifdef ZMUDUO_ENABLE_OPENSSL
      ,
      m_sslContext(nullptr)
#endif
{
    m_connector->setNewConnectionCallback([this](int socketFD) { newConnection(socketFD); });
    ZMUDUO_LOG_FMT_INFO("ctor[%p]", this);
}

TcpClient::~TcpClient() {
    ZMUDUO_LOG_FMT_INFO("dtor[%p]", this);
    TcpConnectionPtr connection;
    bool             unique;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        unique     = m_connection.use_count() == 1;
        connection = m_connection;
    }
    if (connection) {
        assert(m_eventLoop == connection->getEventLoop());
        // FIXME: not 100% safe, if we are in different thread
        m_eventLoop->runInLoop([connection, eventLoop = m_eventLoop]() {
            connection->setCloseCallback([eventLoop](const TcpConnectionPtr& connection) {
                // 关闭连接
                eventLoop->queueInLoop([connection]() { connection->connectDestroyed(); });
            });
        });
        if (unique) {
            connection->forceClose();
        }
    } else {
        m_connector->stop();
    }
#ifdef ZMUDUO_ENABLE_OPENSSL
    if (m_sslContext) {
        SSL_CTX_free(m_sslContext);
    }
#endif
}

#ifdef ZMUDUO_ENABLE_OPENSSL
bool TcpClient::loadCertificates(const std::string& certificatePath,
                                 const std::string& privateKeyPath,
                                 const std::string& caFile /* = "" */,
                                 const std::string& caPath /* = "" */) {
    if (m_connected || m_sslContext) {
        ZMUDUO_LOG_FMT_ERROR("tcpServer[%s] has started", m_name.c_str());
        return false;
    }
    ZMUDUO_LOG_INFO << "Try to set SSL";
    // 注意是client方法
    m_sslContext = SSL_CTX_new(SSLv23_client_method());
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

    // 加载 CA 文件或路径（可选）
    if (!caFile.empty() || !caPath.empty()) {
        CHECK_SSL_ERROR(
            SSL_CTX_load_verify_locations(m_sslContext, caFile.empty() ? nullptr : caFile.c_str(),
                                          caPath.empty() ? nullptr : caPath.c_str()) <= 0,
            "Failed to load CA certificates: %s", ERR_error_string(ERR_get_error(), nullptr));
        // 设置验证选项（可选）
        SSL_CTX_set_verify(m_sslContext, SSL_VERIFY_PEER, nullptr);
        SSL_CTX_set_verify_depth(m_sslContext, 4);
    }

#    undef CHECK_SSL_ERROR
    return true;
}
#endif

void TcpClient::connect() {
    ZMUDUO_LOG_FMT_INFO("TcpClient[%s] connect to %s", m_name.c_str(),
                        m_connector->getServerAddress()->toString().c_str());
    m_connected = true;
    m_connector->start();
}

void TcpClient::stop() {
    m_connected = false;
    m_connector->stop();
}

void TcpClient::disconnect() {
    m_connected = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_connection) {
            m_connection->shutdown();
        }
    }
}

void TcpClient::newConnection(int socketFD) {
    m_eventLoop->assertInLoopThread();
    auto         peerAddrIn  = sockets::getPeerAddress(socketFD);
    Address::Ptr peerAddress = Address::Create(sockets::sockaddr_cast(&peerAddrIn));
    char         buffer[64];
    snprintf(buffer, sizeof(buffer), "%s#%ld", peerAddress->toString().c_str(), S_NEXT_ID++);
    std::string connectName = m_name + buffer;
    ZMUDUO_LOG_FMT_INFO("TcpClient::newConnection[%s] - new connection [%s] from %s",
                        m_name.c_str(), connectName.c_str(), peerAddress->toString().c_str());
    auto         localAddrIn  = sockets::getLocalAddress(socketFD);
    Address::Ptr localAddress = Address::Create(sockets::sockaddr_cast(&localAddrIn));
#ifdef ZMUDUO_ENABLE_OPENSSL
    // ssl
    SSL* ssl = nullptr;
    if (m_sslContext) {
        ssl = SSL_new(m_sslContext);
        SSL_set_fd(ssl, socketFD);
        SSL_set_connect_state(ssl);
    }
    // 创建tcp连接
    TcpConnectionPtr tcpConnection = std::make_shared<TcpConnection>(
        m_eventLoop, connectName, socketFD, localAddress, peerAddress, ssl);
#else
    // 创建tcp连接
    TcpConnectionPtr tcpConnection = std::make_shared<TcpConnection>(
        m_eventLoop, connectName, socketFD, localAddress, peerAddress);
#endif
    // 关闭连接时移除connection
    tcpConnection->setCloseCallback(
        [this](const TcpConnectionPtr& connection) { removeConnection(connection); });
    tcpConnection->setMessageCallback(m_messageCallback);
    tcpConnection->setConnectionCallback(m_connectionCallback);
    tcpConnection->setWriteCompleteCallback(m_writeCompleteCallback);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connection = tcpConnection;
    }
    // 建立连接
    tcpConnection->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnectionPtr& connection) {
    m_eventLoop->assertInLoopThread();
    assert(m_eventLoop == connection->getEventLoop());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        assert(m_connection == connection);
        m_connection.reset();
    }
    m_eventLoop->queueInLoop([connection]() { connection->connectDestroyed(); });
    if (m_retry && m_connected) {
        ZMUDUO_LOG_FMT_INFO("TcpClient[%s] Reconnecting to %s", m_name.c_str(),
                            m_connector->getServerAddress()->toString().c_str());
        // 重新连接
        m_connector->restart();
    }
}

}  // namespace zmuduo::net