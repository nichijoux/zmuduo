#include "zmuduo/net/acceptor.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/socket_options.h"
#include <cassert>
#include <fcntl.h>
#include <unistd.h>

namespace zmuduo::net {
Acceptor::Acceptor(EventLoop* loop, const Address::Ptr& listenAddress, const bool reuseport)
    : m_eventLoop(loop),
      m_acceptSocket(TcpSocket::Create(listenAddress->getFamily())),
      m_acceptChannel(loop, m_acceptSocket.getFD()),
      m_idleFD(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    assert(m_idleFD >= 0);
    // 设置socket选项
    m_acceptSocket.setReuseAddress(true);
    m_acceptSocket.setReusePort(reuseport);
    m_acceptSocket.bind(listenAddress);
    m_acceptChannel.setReadCallback([this](const Timestamp&) { handleRead(); });
}

Acceptor::~Acceptor() {
    // 移除channel
    m_acceptChannel.disableAll();
    m_acceptChannel.remove();
    // 关闭socket
    ::close(m_idleFD);
}

void Acceptor::listen() {
    m_eventLoop->assertInLoopThread();
    m_listening = true;
    // 开始监听连接
    m_acceptSocket.listen();
    // 向poller中注册读事件
    m_acceptChannel.enableReading();
}

void Acceptor::handleRead() {
    // 确保该函数在所属的事件循环线程中执行
    m_eventLoop->assertInLoopThread();
    // 定义一个用于存储远程地址的变量
    Address::Ptr peerAddress;
    // 接受新的连接，返回新的连接文件描述符
    // 如果接受连接成功
    if (const int connectFD = m_acceptSocket.accept(peerAddress); connectFD >= 0) {
        // 如果存在连接回调函数
        if (m_connectCallback) {
            // 调用连接回调函数，传入新的连接文件描述符和远程地址
            m_connectCallback(connectFD, peerAddress);
        } else {
            // 不存在连接回调则关闭
            ::close(connectFD);
        }
        //
    } else {
        ZMUDUO_LOG_FMT_ERROR("Acceptor accept failed");
        // 文件描述符耗尽,通过临时接受并立即关闭一个连接，强制内核清理该连接的资源
        if (errno == EMFILE) {
            // EMFILE指当前进程没有足够的文件描述符来创建新的连接
            // 1. 关闭占位 FD
            ::close(m_idleFD);
            // 2. 接受并立即关闭
            m_idleFD = ::accept(m_acceptSocket.getFD(), nullptr, nullptr);
            ::close(m_idleFD);
            // 3. 重新占位(阻塞打开)
            m_idleFD = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
    }
}
} // namespace zmuduo::net