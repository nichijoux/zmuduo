// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_POLLER_H
#define ZMUDUO_NET_POLLER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include <unordered_map>
#include <vector>

namespace zmuduo::net {
class Channel;
class Poller;
class EventLoop;

/**
 * @class Poller
 * @brief 抽象基类，用于管理事件循环中的 IO 事件。
 *
 * Poller 通过 IO 复用技术（如 epoll、poll、select）监控文件描述符上的事件，
 * 是 Reactor 模型的核心组件，负责事件的分发和管理。
 *
 * @note Poller 是一个抽象类，具体实现由派生类（如 EPollPoller 或 PollPoller）提供。
 * @note 所有方法必须在事件循环线程中调用，以确保线程安全。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Poller* poller = Poller::newPoller(&loop);
 * Channel channel(&loop, fd);
 * poller->updateChannel(&channel);
 * @endcode
 */
class Poller : NoCopyable {
public:
    /**
     * @typedef std::vector&lt;Channel*&gt;
     * @brief 定义活跃 Channel 列表类型，用于存储触发事件的 Channel 指针。
     */
    using ChannelList = std::vector<Channel*>;

    /**
     * @brief 工厂方法，创建 Poller 的具体实现实例。
     * @param[in] loop 所属的事件循环指针。
     * @return 新创建的 Poller 实例指针。
     * @note 根据系统环境变量 ZMUDUO_USE_POLL 和 ZMUDUO_USE_SELECT 选择poller的实现（默认为 epoll ）。
     * @note 请自行管理poller的内存
     */
    static Poller* newPoller(EventLoop* loop);

public:
    /**
     * @brief 构造函数，初始化 Poller 实例。
     * @param[in] eventLoop 所属的事件循环指针。
     */
    explicit Poller(EventLoop* eventLoop)
        : m_ownerLoop(eventLoop) {}

    /**
     * @brief 虚析构函数，确保派生类正确清理资源。
     */
    virtual ~Poller() = default;

    /**
     * @brief 监控文件描述符上的 IO 事件，并返回活跃的 Channel 列表。
     * @param[in] timeoutMs 超时时间（毫秒），-1 表示无限期等待。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @return 事件发生的时间戳。
     * @note 必须由派生类实现，调用需在事件循环线程中。
     */
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

    /**
     * @brief 更新 Channel 的监听事件。
     * @param[in] channel 需要更新的 Channel 指针。
     * @note 必须由派生类实现，调用需在事件循环线程中。
     */
    virtual void updateChannel(Channel* channel) = 0;

    /**
     * @brief 移除指定的 Channel。
     * @param[in] channel 需要移除的 Channel 指针。
     * @note 必须由派生类实现，调用需在事件循环线程中。
     */
    virtual void removeChannel(Channel* channel) = 0;

    /**
     * @brief 检查指定 Channel 是否由当前 Poller 管理。
     * @param[in] channel 需要检查的 Channel 指针。
     * @return 如果 Channel 由当前 Poller 管理，返回 true；否则返回 false。
     */
    bool hasChannel(const Channel* channel) const;

protected:
    /**
     * @brief 文件描述符到 Channel 的映射。
     * @note 用于存储当前 Poller 管理的所有 Channel，以文件描述符为键。
     */
    using ChannelMap = std::unordered_map<int, Channel*>;

    /**
     * @brief 断言当前线程为事件循环所属线程。
     * @throw 如果当前线程不是事件循环所属线程，抛出致命日志错误。
     */
    void assertInLoopThread() const;

    ChannelMap m_channels; ///< 文件描述符到 Channel 的映射
private:
    EventLoop* m_ownerLoop; ///< 所属的事件循环
};
} // namespace zmuduo::net

#endif