// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_EPOLL_POLLER_H
#define ZMUDUO_EPOLL_POLLER_H
#include "zmuduo/net/poller.h"
#include <sys/epoll.h>

namespace zmuduo::net {
/**
 * @class EpollPoller
 * @brief 基于 Linux epoll 的 I/O 多路复用器，管理事件循环中的 Channel。
 *
 * EpollPoller 是 Reactor 模型中的核心组件，继承自 Poller 基类，使用 Linux epoll API
 * 实现高效的事件轮询。<br/> 负责管理文件描述符的注册、更新和移除，轮询活跃事件，并通知对应的
 * Channel 处理读写事件。<br/> 支持动态扩容事件列表以适应高并发场景。
 *
 * @note 所有方法必须在所属 EventLoop 线程中调用，以确保线程安全。
 * @note 使用 EPOLL_CLOEXEC 标志创建 epoll 实例，避免文件描述符泄漏。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * EpollPoller poller(&loop);
 * Channel channel(&loop, socketFD);
 * channel.setReadCallback([] { std::cout << "Read event" << std::endl; });
 * channel.enableReading();
 * poller.updateChannel(&channel);
 * loop.loop();
 * @endcode
 */
class EpollPoller final : public Poller {
public:
    /**
     * @brief 构造函数，初始化 epoll 实例。
     * @param[in] loop 所属的事件循环。
     * @throw 如果 epoll_create1 失败，记录致命错误并终止。
     */
    explicit EpollPoller(EventLoop* loop);

    /**
     * @brief 析构函数，关闭 epoll 文件描述符。
     */
    ~EpollPoller() override;

    /**
     * @brief 轮询活跃事件并返回当前时间戳。
     * @param[in] timeoutMs 超时时间（毫秒），-1 表示无限等待。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @return 当前时间戳。
     * @note 如果事件数量达到事件列表容量，自动扩容。
     */
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

    /**
     * @brief 更新 Channel 的事件状态。
     * @param[in] channel 要更新的 Channel。
     * @note 根据 Channel 状态（NEW, ADDED, DELETED）执行 epoll_ctl 操作（ADD, MOD, DEL）。
     * @throw 如果不在事件循环线程中，抛出断言错误。
     */
    void updateChannel(Channel* channel) override;

    /**
     * @brief 从 epoll 中移除 Channel。
     * @param[in] channel 要移除的 Channel。
     * @note 从 Channel 映射中移除并执行 epoll_ctl DEL 操作（如果需要）。
     * @throw 如果不在事件循环线程中或 Channel 状态不合法，抛出断言错误。
     */
    void removeChannel(Channel* channel) override;

private:
    /**
     * @typedef std::vector&lt;epoll_event&gt;
     * @brief 事件列表类型，存储 epoll 事件。
     */
    using EventList = std::vector<epoll_event>;

    /**
     * @brief 填充活跃的 Channel 列表。
     * @param[in] numEvents 触发的事件数量。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @throw 如果事件数量超过列表容量或 Channel 不存在，抛出断言错误。
     */
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    /**
     * @brief 更新 epoll 的事件状态。
     * @param[in] operation epoll_ctl 操作（EPOLL_CTL_ADD, EPOLL_CTL_MOD, EPOLL_CTL_DEL）。
     * @param[in] channel 关联的 Channel。
     * @note 调用 epoll_ctl 更新文件描述符的事件。
     * @throw 如果 epoll_ctl 失败，记录错误（DEL 操作记录 ERROR，其他记录 FATAL）。
     */
    void update(int operation, Channel* channel) const;

private:
    static constexpr int S_INIT_EVENT_LIST_SIZE = 16; ///< 事件列表的初始容量
    int                  m_epollFD;                   ///< epoll 文件描述符
    EventList            m_events;                    ///< 存储 epoll 事件的列表
};
} // namespace zmuduo::net

#endif