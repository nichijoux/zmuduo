#include "zmuduo/net/poller/epoll_poller.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include <cassert>
#include <strings.h>
#include <unistd.h>

namespace zmuduo::net {
enum EpollPollerState {
    // channel未添加到poller中
    NEW = -1,
    // channel已添加到poller中
    ADDED = 1,
    // channel从poller中删除
    DELETED
};

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      m_epollFD(::epoll_create1(EPOLL_CLOEXEC)),
      m_events(S_INIT_EVENT_LIST_SIZE) {
    // 判断是否创建成功
    if (m_epollFD < 0) {
        ZMUDUO_LOG_FMT_FATAL("EPollPoller::EPollPoller");
    }
}

EpollPoller::~EpollPoller() {
    ::close(m_epollFD);
}

Timestamp EpollPoller::poll(const int timeoutMs, ChannelList* activeChannels) {
    ZMUDUO_LOG_FMT_INFO("epollFD is %d,total fd count is %ld", m_epollFD, m_channels.size());
    const int numEvents =
        ::epoll_wait(m_epollFD, m_events.data(), static_cast<int>(m_events.size()), timeoutMs);
    const auto now = Timestamp::Now();
    if (numEvents > 0) {
        ZMUDUO_LOG_FMT_INFO("%d events happended", numEvents);
        // 有事件发生
        fillActiveChannels(numEvents, activeChannels);
        if (static_cast<size_t>(numEvents) == m_events.size()) {
            // 需要扩容
            m_events.resize(m_events.size() * 2);
        }
    } else if (numEvents == 0) {
        // 没有事件发生
        ZMUDUO_LOG_FMT_INFO("EPollPoller no events happend");
    } else {
        // 错误
        if (errno != EINTR) {
            ZMUDUO_LOG_FMT_ERROR("EPollPoller::poll() %d", errno);
        }
    }
    return now;
}

void EpollPoller::updateChannel(Channel* channel) {
    assertInLoopThread();
    ZMUDUO_LOG_FMT_INFO("fd is %d,events is %d,state is %d", channel->getFD(), channel->getEvents(),
                        channel->getIndex());
    // 获取当前channel的状态
    const int index = channel->getIndex();
    const int fd    = channel->getFD();
    if (index == NEW || index == DELETED) {
        if (index == NEW) {
            // new
            assert(m_channels.find(fd) == m_channels.end());
            m_channels[fd] = channel;
        } else {
            // deleted
            assert(m_channels.find(fd) != m_channels.end());
            assert(m_channels[fd] == channel);
        }
        channel->setIndex(ADDED);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // update existing one with EPOLL_CTL_MOD/DEL
        assert(hasChannel(channel));
        assert(index == EpollPollerState::ADDED);
        // 如果channel没有事件,那就删除
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(DELETED);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EpollPoller::removeChannel(Channel* channel) {
    assertInLoopThread();
    const int fd = channel->getFD();
    ZMUDUO_LOG_FMT_INFO("channel's fd is %d", fd);
    // channel必须存在且事件应该为空
    assert(hasChannel(channel));
    assert(channel->isNoneEvent());
    // 获取当前channel状态
    const int index = channel->getIndex();
    assert(index == ADDED || index == DELETED);
    // 从map中删除
    const size_t n = m_channels.erase(fd);
    assert(n == 1);

    if (index == ADDED) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(NEW);
}

void EpollPoller::fillActiveChannels(const int numEvents, ChannelList* activeChannels) const {
    assert(static_cast<size_t>(numEvents) <= m_events.size());
    for (int i = 0; i < numEvents; ++i) {
        auto* channel = static_cast<Channel*>(m_events[i].data.ptr);
        // 获取channel的fd
        int  fd = channel->getFD();
        auto it = m_channels.find(fd);
        assert(it != m_channels.end());
        assert(it->second == channel);
        // 通知channel发生了事件
        channel->setHappenedEvents(m_events[i].events);
        activeChannels->emplace_back(channel);
    }
}

void EpollPoller::update(const int operation, Channel* channel) const {
    epoll_event event{};
    bzero(&event, sizeof(event));
    // channel注册的事件
    event.events   = channel->getEvents();
    event.data.ptr = channel;
    // 获取channel的fd
    // 更新epoll的事件
    if (const int fd = channel->getFD(); epoll_ctl(m_epollFD, operation, fd, &event) < 0) {
        // epoll_ctl失败
        if (operation == EPOLL_CTL_DEL) {
            ZMUDUO_LOG_FMT_ERROR("epoll_ctl del error %d", errno);
        } else {
            ZMUDUO_LOG_FMT_FATAL("epoll_ctl operation error %d", errno);
        }
    }
}
} // namespace zmuduo::net