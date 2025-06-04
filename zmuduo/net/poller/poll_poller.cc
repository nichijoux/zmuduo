#include "zmuduo/net/poller/poll_poller.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include <cassert>

namespace zmuduo::net {
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    Poller::assertInLoopThread();
    ZMUDUO_LOG_FMT_INFO("total fd count is %ld", m_channels.size());
    // 使用 poll 系统调用等待文件描述符上的事件发生,numEvents不会超过m_pollFDs.size()
    int numEvents = ::poll(m_pollFDs.data(), m_pollFDs.size(), timeoutMs);
    // 获取当前时间戳
    auto now = Timestamp::Now();
    // 如果监听到的事件数量大于 0，表示有事件发生
    if (numEvents > 0) {
        // 监听到事件发生
        fillActiveChannels(numEvents, activeChannels);
    } else if (numEvents == 0) {
        ZMUDUO_LOG_FMT_INFO("PollPoller no events happend");
    } else {
        if (errno != EINTR) {
            ZMUDUO_LOG_FMT_ERROR("PollPoller::poll() %d", errno);
        }
    }
    return now;
}

void PollPoller::updateChannel(Channel* channel) {
    Poller::assertInLoopThread();
    int index = channel->getIndex();
    ZMUDUO_LOG_FMT_INFO("fd is %d,index is %d", channel->getFD(), index);
    if (index < 0) {
        // 当前channel不存在对应的pollfd,创建新的pollfd
        assert(m_channels.find(channel->getFD()) == m_channels.end());
        pollfd pollFD = {.fd      = channel->getFD(),
                         .events  = static_cast<short>(channel->getEvents()),
                         .revents = 0};
        m_pollFDs.emplace_back(pollFD);
        channel->setIndex(static_cast<int>(m_pollFDs.size() - 1));
        m_channels[pollFD.fd] = channel;
    } else {
        assert(hasChannel(channel));
        assert(0 <= index && index < static_cast<int>(m_pollFDs.size()));
        // 更新pollfd
        auto& pollFD = m_pollFDs[index];
        assert(pollFD.fd == channel->getFD() || pollFD.fd == -channel->getFD() - 1);
        pollFD.events  = static_cast<short>(channel->getEvents());
        pollFD.revents = 0;
        if (channel->isNoneEvent()) {
            // 如果当前pollfd不存在事件,则忽略fd
            pollFD.fd = -channel->getFD() - 1;
        }
    }
}

void PollPoller::removeChannel(Channel* channel) {
    Poller::assertInLoopThread();
    // channel必须存在且事件应该为空
    assert(hasChannel(channel));
    assert(channel->isNoneEvent());
    // 获取对应的pollFD
    int index = channel->getIndex();
    ZMUDUO_LOG_FMT_INFO("channel's fd is %d,index is %d", channel->getFD(), index);
    assert(0 <= index && index < static_cast<int>(m_pollFDs.size()));
    const auto& pollFD = m_pollFDs[index];
    assert(pollFD.fd == channel->getFD() || pollFD.fd == -channel->getFD() - 1);
    // 移除当前pollfd
    size_t n = m_channels.erase(channel->getFD());
    assert(n == 1);
    if (static_cast<size_t>(index) == m_pollFDs.size() - 1) {
        m_pollFDs.pop_back();
    } else {
        // 如果是在中间,则将最后一个pollfd移动到当前位置,并删除最后一个pollfd
        int fdAtEnd = m_pollFDs.back().fd;
        iter_swap(m_pollFDs.begin() + index, m_pollFDs.end() - 1);
        if (fdAtEnd < 0) {
            fdAtEnd = -fdAtEnd - 1;
        }
        m_channels[fdAtEnd]->setIndex(index);
        m_pollFDs.pop_back();
    }
}

void PollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (const auto& pollFD : m_pollFDs) {
        if (pollFD.revents > 0 && numEvents > 0) {
            // 更新activeChannels
            auto it      = m_channels.find(pollFD.fd);
            auto channel = it->second;
            assert(hasChannel(channel));
            // 通知channel发生了事件
            channel->setHappenedEvents(pollFD.revents);
            activeChannels->emplace_back(channel);
            --numEvents;
            if (numEvents == 0) {
                return;
            }
        }
    }
}
}  // namespace zmuduo::net
