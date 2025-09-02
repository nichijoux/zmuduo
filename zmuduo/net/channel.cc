#include "zmuduo/net/channel.h"
#include "zmuduo/net/event_loop.h"
#include <poll.h>
#include <zmuduo/base/logger.h>

namespace zmuduo::net {
Channel::Channel(EventLoop* eventLoop, const int fd)
    : m_ownerLoop(eventLoop),
      m_fd(fd) {}

void Channel::handleEvent(const Timestamp& receiveTime) const {
    if (m_tied) {
        // 绑定过,获取一个shared指针
        // 如果还存活
        if (std::shared_ptr<void> guard = m_tie.lock()) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::tie(const std::shared_ptr<void>& object) {
    m_tie  = object;
    m_tied = true;
}

void Channel::remove() {
    m_ownerLoop->removeChannel(this);
}

void Channel::update() {
    // 通过channel所属的eventloop调用poller相应的方法
    m_ownerLoop->updateChannel(this);
}

void Channel::handleEventWithGuard(const Timestamp& receiveTime) const {
    ZMUDUO_LOG_FMT_INFO("channel handleEvent revents:%d", m_happenedEvents);
    if ((m_happenedEvents & POLLHUP) && !(m_happenedEvents & EPOLLIN)) {
        if (m_closeCallback) {
            m_closeCallback();
        }
    }
    if (m_happenedEvents & POLLNVAL) {
        ZMUDUO_LOG_WARNING << "fd = " << m_fd << " Channel::handleEventWithGuard() POLLNVAL";
    }
    // 错误事件
    if (m_happenedEvents & (EPOLLERR | POLLNVAL)) {
        if (m_errorCallback) {
            m_errorCallback();
        }
    }
    // 可读事件
    if (m_happenedEvents & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (m_readCallback) {
            m_readCallback(receiveTime);
        }
    }
    // 可写事件
    if (m_happenedEvents & EPOLLOUT) {
        if (m_writeCallback) {
            m_writeCallback();
        }
    }
}
} // namespace zmuduo::net