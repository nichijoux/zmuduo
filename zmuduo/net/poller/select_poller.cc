#include "zmuduo/net/poller/select_poller.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"
#include <cassert>

namespace zmuduo::net {
Timestamp SelectPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    fd_set readSet, writeSet, exceptSet;
    int    maxFD = -1;
    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&exceptSet);
    // 初始化fd_set
    for (const auto& selectFD : m_selectFDs) {
        int      fd     = std::get<0>(selectFD);
        uint32_t events = std::get<1>(selectFD);
        if (fd >= 0 && (events & (EPOLLIN | EPOLLPRI))) {
            FD_SET(fd, &readSet);
        }
        if (fd >= 0 && (events & EPOLLOUT)) {
            FD_SET(fd, &writeSet);
        }
        FD_SET(fd, &exceptSet);
        maxFD = std::max(maxFD, fd);
    }
    // 超时时间
    timeval timeout   = {.tv_sec = timeoutMs / 1000, .tv_usec = timeoutMs % 1000 * 1000};
    int     numEvents = ::select(maxFD + 1, &readSet, &writeSet, &exceptSet, &timeout);
    auto    now       = Timestamp::Now();
    if (numEvents > 0) {
        // select监听到事件发生
        fillActiveChannels(numEvents, activeChannels, readSet, writeSet, exceptSet);
    } else if (numEvents == 0) {
        ZMUDUO_LOG_FMT_INFO("SelectPoller no events happened");
    } else {
        if (errno != EINTR) {
            ZMUDUO_LOG_FMT_ERROR("SelectPoller::poll() %d", errno);
        }
    }
    return now;
}

void SelectPoller::updateChannel(Channel* channel) {
    Poller::assertInLoopThread();
    int index = channel->getIndex();
    ZMUDUO_LOG_FMT_INFO("fd is %d,index is %d", channel->getFD(), index);
    if (index < 0) {
        // 当前channel不存在对应的selectFD,修改selectFD
        assert(m_channels.find(channel->getFD()) == m_channels.end());
        // 将selectFD添加到selectFDs
        m_selectFDs.emplace_back(channel->getFD(), channel->getEvents());
        channel->setIndex(static_cast<int>(m_selectFDs.size() - 1));
        m_channels[channel->getFD()] = channel;
    } else {
        // 已经存在
        assert(hasChannel(channel));
        assert(0 <= index && index < static_cast<int>(m_selectFDs.size()));
        // 更新selectFD的事件
        auto& selectFD = m_selectFDs[index];
        assert(std::get<0>(selectFD) == channel->getFD() ||
               std::get<0>(selectFD) == -channel->getFD() - 1);
        std::get<1>(selectFD) = channel->getEvents();
        if (channel->isNoneEvent()) {
            // 如果当前fd不存在事件,则忽略fd
            std::get<0>(selectFD) = -channel->getFD() - 1;
        }
    }
}

void SelectPoller::removeChannel(Channel* channel) {
    Poller::assertInLoopThread();
    int fd = channel->getFD();
    ZMUDUO_LOG_FMT_INFO("SelectPoller channel's fd is %d", fd);
    // channel必须存在且事件应该为空
    assert(hasChannel(channel));
    assert(channel->isNoneEvent());
    // 获取当前channel状态
    int index = channel->getIndex();
    assert(0 <= index && index < static_cast<int>(m_selectFDs.size()));
    // 从map将当前channel删除
    size_t n = m_channels.erase(fd);
    assert(n == 1);
    // 删除selectFD
    if (index == m_selectFDs.size() - 1) {
        // channel刚好在末尾
        m_selectFDs.pop_back();
    } else {
        // channel不在末尾,将末尾的channel放到当前位置
        int fdAtEnd = std::get<0>(m_selectFDs.back());
        iter_swap(m_selectFDs.begin() + index, m_selectFDs.end() - 1);
        if (fdAtEnd < 0) {
            fdAtEnd = -fdAtEnd - 1;
        }
        m_channels[fdAtEnd]->setIndex(index);  // 更新末尾channel的index
        m_selectFDs.pop_back();
    }
}

void SelectPoller::fillActiveChannels(int          numEvents,
                                      ChannelList* activeChannels,
                                      fd_set&      readSet,
                                      fd_set&      writeSet,
                                      fd_set&      exceptSet) const {
    for (const auto& selectFD : m_selectFDs) {
        int  fd = std::get<0>(selectFD);
        auto it = m_channels.find(fd);
        assert(it != nullptr);
        auto channel = it->second;
        assert(hasChannel(channel));
        // 获取revents
        int revents = 0;
        // select将更新这个集合,把其中不可读的套节字去掉
        // 只保留符合条件的套节字在这个集合里面
        if (fd >= 0 && (FD_ISSET(fd, &readSet))) {
            revents |= EPOLLIN;
        }
        if (fd >= 0 && FD_ISSET(fd, &writeSet)) {
            revents |= EPOLLOUT;
        }
        if (fd >= 0 && FD_ISSET(fd, &exceptSet)) {
            revents |= EPOLLERR;
        }
        // 设置channel有事件发生
        if (revents != 0) {
            channel->setHappenedEvents(revents);
            activeChannels->emplace_back(channel);
            --numEvents;
            if (numEvents == 0) {
                return;
            }
        }
    }
}
}  // namespace zmuduo::net