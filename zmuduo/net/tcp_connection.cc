#include "zmuduo/net/tcp_connection.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/socket.h"
#include "zmuduo/net/socket_options.h"
#include <cassert>
#include <openssl/err.h>
#include <unistd.h>
#include <utility>

namespace zmuduo::net {
void defaultConnectionCallback(const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_FMT_INFO("%s -> %s is %s", connection->getLocalAddress()->toString().c_str(),
                        connection->getPeerAddress()->toString().c_str(),
                        connection->isConnected() ? "UP" : "DOWN");
}

void defaultMessageCallback(const TcpConnectionPtr&, Buffer& buffer, const Timestamp&) {
    buffer.retrieveAll();
}

TcpConnection::TcpConnection(EventLoop*   loop,
                             std::string  name,
                             int          socketFD,
                             Address::Ptr localAddress,
                             Address::Ptr peerAddress
#ifdef ZMUDUO_ENABLE_OPENSSL
                             ,
                             SSL* ssl
#endif
                             )
    : m_eventLoop(EventLoop::checkNotNull(loop)),
      m_name(std::move(name)),
      m_socket(new TcpSocket(socketFD)),
      m_channel(new Channel(loop, socketFD)),
      m_localAddress(std::move(localAddress)),
      m_peerAddress(std::move(peerAddress)),
      m_state(State::CONNECTING),
      m_reading(true),
      m_highWaterMark(64 * 1024 * 1024)
#ifdef ZMUDUO_ENABLE_OPENSSL
      ,
      m_ssl(ssl),
      m_sslState(ssl ? SSLState::HANDSHAKING : SSLState::NONE)
#endif
{
    m_channel->setReadCallback([this](const auto& time) { handleRead(time); });
    m_channel->setWriteCallback([this] { handleWrite(); });
    m_channel->setCloseCallback([this] { handleClose(); });
    m_channel->setErrorCallback([this] { handleError(); });
    ZMUDUO_LOG_FMT_DEBUG("TcpConnection::ctor[%s] at %d", m_name.c_str(), socketFD);
    m_socket->setKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    ZMUDUO_LOG_FMT_DEBUG("TcpConnection::dtor[%s] at %d", m_name.c_str(), m_channel->getFD());
    assert(m_state == State::DISCONNECTED);

#ifdef ZMUDUO_ENABLE_OPENSSL
    if (m_ssl) {
        ZMUDUO_LOG_FMT_DEBUG("clean ssl");
        m_socket.reset();
        SSL_shutdown(m_ssl);
        SSL_free(m_ssl);
        m_ssl = nullptr;
    }
#endif
}

bool TcpConnection::getTcpInfo(tcp_info* info) const {
    return m_socket->getTcpInfo(info);
}

void TcpConnection::send(const std::string& message) {
    if (m_state == State::CONNECTED) {
        // 如果已经成功连接
        m_eventLoop->runInLoop([this, message] {
            // 捕获 message并发送
            sendInLoop(message.c_str(), message.length());
        });
    }
}

void TcpConnection::send(Buffer& buffer) {
    if (m_state == State::CONNECTED) {
        m_eventLoop->runInLoop(
            [this, &buffer] { sendInLoop(buffer.peek(), buffer.getReadableBytes()); });
    }
}

void TcpConnection::shutdown() {
    if (m_state == State::CONNECTED) {
        m_state = State::DISCONNECTING;
#ifdef ZMUDUO_ENABLE_OPENSSL
        m_sslState = SSLState::NONE;
#endif
        // 确保shutdownInLoop在IO线程中执行
        m_eventLoop->runInLoop([this] { shutdownInLoop(); });
    }
}

void TcpConnection::forceClose() {
    if (m_state == State::CONNECTED || m_state == State::DISCONNECTING) {
        m_state = State::DISCONNECTING;
#ifdef ZMUDUO_ENABLE_OPENSSL
        m_sslState = SSLState::NONE;
#endif
        m_eventLoop->queueInLoop([this] { forceCloseInLoop(); });
    }
}

void TcpConnection::setTcpNoDelay(bool on) {
    m_socket->setTcpNoDelay(on);
}

void TcpConnection::startRead() {
    m_eventLoop->runInLoop([this] { startReadInLoop(); });
}

void TcpConnection::stopRead() {
    m_eventLoop->runInLoop([this] { stopReadInLoop(); });
}

void TcpConnection::connectEstablished() {
    m_eventLoop->assertInLoopThread();
    assert(m_state == State::CONNECTING);
    m_state = State::CONNECTED;
    m_channel->tie(shared_from_this());
#ifdef ZMUDUO_ENABLE_OPENSSL
    // 准备建立ssl握手
    if (m_sslState == SSLState::HANDSHAKING) {
        continueSSLHandshake();
        return;
    }
#endif
    // 新连接建立，执行回调
    if (m_connectionCallback) {
        m_connectionCallback(shared_from_this());
    }
    // 启用读事件应该放在后面，因为connection中的函数可能没有完成
    m_channel->enableReading();
}

void TcpConnection::connectDestroyed() {
    m_eventLoop->assertInLoopThread();
    if (m_state == State::CONNECTED) {
        m_state = State::DISCONNECTED;
        m_channel->disableAll();
        // 关闭连接的回调
        if (m_connectionCallback) {
            m_connectionCallback(shared_from_this());
        }
    }
    // 把channel从poller中删除
    m_channel->remove();
}

#ifdef ZMUDUO_ENABLE_OPENSSL

void TcpConnection::continueSSLHandshake() {
    assert(m_sslState == SSLState::HANDSHAKING);
    int ret = SSL_do_handshake(m_ssl);
    if (ret == 1) {
        m_sslState = SSLState::CONNECTED;
        ZMUDUO_LOG_FMT_IMPORTANT("SSL handshake success for %s", m_name.c_str());
        if (!SSL_is_server(m_ssl)) {
            // 客户端验证服务器证书
            X509* cert = SSL_get_peer_certificate(m_ssl);
            if (cert) {
                char* subject = X509_NAME_oneline(X509_get_subject_name(cert), nullptr, 0);
                char* issuer  = X509_NAME_oneline(X509_get_issuer_name(cert), nullptr, 0);

                ZMUDUO_LOG_DEBUG << "服务器证书信息:";
                ZMUDUO_LOG_DEBUG << "  主题: " << subject;
                ZMUDUO_LOG_DEBUG << "  颁发者: " << issuer;

                OPENSSL_free(subject);
                OPENSSL_free(issuer);
                X509_free(cert);

                long verify_result = SSL_get_verify_result(m_ssl);
                if (verify_result != X509_V_OK) {
                    ZMUDUO_LOG_ERROR << "证书验证失败: "
                                     << X509_verify_cert_error_string(verify_result);
                    handleClose();
                }
            } else {
                ZMUDUO_LOG_ERROR << "未收到服务器证书";
                handleClose();
            }
        }
        // SSL完成连接建立，执行回调
        if (m_connectionCallback) {
            m_connectionCallback(shared_from_this());
        }
        m_channel->enableReading();
    } else {
        int err = SSL_get_error(m_ssl, ret);
        if (err == SSL_ERROR_WANT_READ) {
            m_channel->enableReading();
        } else if (err == SSL_ERROR_WANT_WRITE) {
            m_channel->enableWriting();
        } else {
            m_sslState = SSLState::FAILED;
            ZMUDUO_LOG_FMT_ERROR("SSL handshake failed [%s], err=%d", m_name.c_str(), err);
            ERR_print_errors_fp(stderr);
            handleClose();
        }
    }
}
#endif

void TcpConnection::handleRead(const Timestamp& receiveTime) {
    m_eventLoop->assertInLoopThread();
#ifdef ZMUDUO_ENABLE_OPENSSL
    if (m_ssl && m_sslState == SSLState::HANDSHAKING) {
        continueSSLHandshake();
        return;
    }
#endif
    int savedErrno = 0;
    // 通过buffer读取数据
#ifdef ZMUDUO_ENABLE_OPENSSL
    ssize_t n = m_ssl ? m_inputBuffer.readSSL(m_ssl, &savedErrno) :
                        m_inputBuffer.readFD(m_channel->getFD(), &savedErrno);
#else
    ssize_t n = m_inputBuffer.readFD(m_channel->getFD(), &savedErrno);
#endif
    // 收到数据
    if (n > 0) {
        if (m_messageCallback) {
            m_messageCallback(shared_from_this(), m_inputBuffer, receiveTime);
        }
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        if (errno != EAGAIN) {
            ZMUDUO_LOG_FMT_ERROR("TcpConnection::handleRead,errno %d", errno);
            handleError();
        }
    }
}

void TcpConnection::handleWrite() {
    m_eventLoop->assertInLoopThread();
#ifdef ZMUDUO_ENABLE_OPENSSL
    if (m_ssl && m_sslState == SSLState::HANDSHAKING) {
        continueSSLHandshake();
        return;
    }
#endif
    if (m_channel->isWriting()) {
        int savedErrno;
#ifdef ZMUDUO_ENABLE_OPENSSL
        ssize_t n = m_ssl ? m_outputBuffer.writeSSL(m_ssl, &savedErrno) :
                            m_outputBuffer.writeFD(m_channel->getFD(), &savedErrno);
#else
        ssize_t n = m_outputBuffer.writeFD(m_channel->getFD(), &savedErrno);
#endif
        if (n > 0) {
            m_outputBuffer.retrieve(n);
            if (m_outputBuffer.getReadableBytes() == 0) {
                // 发送完成
                m_channel->disableWriting();
                // 写入完成的回调
                if (m_writeCompleteCallback) {
                    m_eventLoop->queueInLoop(
                        [this] { m_writeCompleteCallback(shared_from_this()); });
                }
                if (m_state == State::DISCONNECTING) {
                    shutdownInLoop();
                }
            }
        } else if (n == 0) {
            handleClose();
        } else {
            errno = savedErrno;
            ZMUDUO_LOG_FMT_ERROR("TcpConnection::handleWrite,errno %d", errno);
            handleError();
        }
    } else {
        ZMUDUO_LOG_FMT_ERROR("Connection fd =%d is down, but still in writing", m_channel->getFD());
    }
}

void TcpConnection::handleClose() {
    m_eventLoop->assertInLoopThread();
    ZMUDUO_LOG_FMT_DEBUG("fd = %d,state = %d", m_channel->getFD(), static_cast<int>(m_state));
    assert(m_state == State::CONNECTED || m_state == State::DISCONNECTING);
    m_state = State::DISCONNECTED;
#ifdef ZMUDUO_ENABLE_OPENSSL
    m_sslState = SSLState::NONE;
#endif
    m_channel->disableAll();
    // 获取当前指针，防止回调函数中TcpConnection被释放
    TcpConnectionPtr guardThis(shared_from_this());
    if (m_connectionCallback) {
        m_connectionCallback(guardThis);
    }
    if (m_closeCallback) {
        m_closeCallback(guardThis);
    }
}

void TcpConnection::handleError() {
    // 获取错误码
    int savedErrno = sockets::getSocketError(m_channel->getFD());
    if (savedErrno == EPIPE || savedErrno == ECONNRESET) {
        forceClose();
    } else {
        ZMUDUO_LOG_FMT_ERROR("TcpConnection::doWhenError [%s] - SO_ERROR = %d %s", m_name.c_str(),
                             savedErrno, strerror(savedErrno));
    }
}

void TcpConnection::sendInLoop(const void* message, size_t length) {
    if (length == 0) {
        return;
    }
    m_eventLoop->assertInLoopThread();
    ssize_t nwrote     = 0;
    size_t  remaining  = length;
    bool    faultErrno = false;
    if (m_state == State::DISCONNECTED) {
        ZMUDUO_LOG_FMT_WARNING("disconnected, give up writing");
        return;
    }
    if (!m_channel->isWriting() && m_outputBuffer.getReadableBytes() == 0) {
        // channel第一次开始写数据且缓冲区没有待发送的数据
#ifdef ZMUDUO_ENABLE_OPENSSL
        nwrote = m_ssl ? ::SSL_write(m_ssl, message, static_cast<int>(length)) :
                         ::write(m_channel->getFD(), message, length);
#else
        nwrote = ::write(m_channel->getFD(), message, length);
#endif
        if (nwrote >= 0) {
            // 更新剩余待发送的数据
            remaining = length - nwrote;
            if (remaining == 0 && m_writeCompleteCallback) {
                // 发送完成
                m_eventLoop->queueInLoop([this] { m_writeCompleteCallback(shared_from_this()); });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                ZMUDUO_LOG_FMT_ERROR("TcpConnection::sendInLoop");
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultErrno = true;
                }
            }
        }
    }

    assert(remaining <= length);
    if (!faultErrno && remaining > 0) {
        // 没有将message全部发送出去,需要将数据保存到缓冲区中,并给channel注册EPOLLOUT事件
        // poller发现缓冲区有空间会通知相应channel调用writeCallback方法
        // 也就是调用TcpConnection::handleWrite方法
        // 获取目前缓冲区剩余待发送的数据长度
        size_t oldLen = m_outputBuffer.getReadableBytes();
        if (oldLen + remaining >= m_highWaterMark && oldLen < m_highWaterMark &&
            m_highWaterMarkCallback) {
            m_eventLoop->queueInLoop([this, oldLen, remaining] {
                m_highWaterMarkCallback(shared_from_this(), oldLen + remaining);
            });
        }
        // 添加没有发送的数据到缓冲区
        m_outputBuffer.write(static_cast<const char*>(message) + nwrote, remaining);
        // 如果对写事件不感兴趣则注册写事件
        if (!m_channel->isWriting()) {
            m_channel->enableWriting();
        }
    }
}

void TcpConnection::forceCloseInLoop() {
    m_eventLoop->assertInLoopThread();
    // 已经连接或者正在取消连接
    if (m_state == State::CONNECTED || m_state == State::DISCONNECTING) {
        handleClose();
    }
}

void TcpConnection::shutdownInLoop() {
    m_eventLoop->assertInLoopThread();
    // 没有注册写事件，说明缓冲区没有待发送的数据
    if (!m_channel->isWriting()) {
        // 关闭写端（会注册EPOLLHUP事件）=> 会调用到TcpConnection的handleClose方法
#ifdef ZMUDUO_ENABLE_OPENSSL
        if (m_ssl) {
            if (::SSL_shutdown(m_ssl) == 0) {
                // 半关闭（还要再调用一次）
                ::SSL_shutdown(m_ssl);
            }
        }
#endif
        // 只关闭读端，这是为了读取对端关闭前发生的数据
        m_socket->shutdownWrite();
    }
}

void TcpConnection::startReadInLoop() {
    m_eventLoop->assertInLoopThread();
    // 启用channel的读取功能
    if (!m_reading || !m_channel->isReading()) {
        m_channel->enableReading();
        m_reading = true;
    }
}

void TcpConnection::stopReadInLoop() {
    m_eventLoop->assertInLoopThread();
    if (m_reading || m_channel->isReading()) {
        m_channel->disableReading();
        m_reading = false;
    }
}
}  // namespace zmuduo::net
