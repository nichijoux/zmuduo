// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_POLL_POLLER_H
#define ZMUDUO_POLL_POLLER_H
#include "zmuduo/net/poller.h"
#include <poll.h>
#include <vector>

namespace zmuduo::net {
/**
 * @class PollPoller
 * @brief 基于 POSIX poll 的 I/O 多路复用器，管理事件循环中的 Channel。
 *
 * PollPoller 是 Reactor 模型中的核心组件，继承自 Poller 基类，使用 POSIX poll
 * 系统调用实现事件轮询。<br/> 负责管理文件描述符的注册、更新和移除，轮询活跃事件，并通知对应的
 * Channel 处理读写事件。<br/> 使用 std::vector<struct pollfd>
 * 维护文件描述符列表，支持动态添加和移除 Channel。
 *
 * @note 所有方法必须在所属 EventLoop 线程中调用，以确保线程安全。
 * @note 相比 EpollPoller，PollPoller 更适合小型连接场景，但在高并发下性能较低。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * PollPoller poller(&loop);
 * Channel channel(&loop, socketFD);
 * channel.setReadCallback([] { std::cout << "Read event" << std::endl; });
 * channel.enableReading();
 * poller.updateChannel(&channel);
 * loop.loop();
 * @endcode
 */
class PollPoller final : public Poller {
public:
    /**
     * @brief 构造函数，初始化 PollPoller。
     * @param[in] eventLoop 所属的事件循环。
     */
    explicit PollPoller(EventLoop* eventLoop)
        : Poller(eventLoop) {}

    /**
     * @brief 析构函数，清理资源。
     * @note 默认析构函数，自动释放 pollfd 列表。
     */
    ~PollPoller() override = default;

    /**
     * @brief 轮询活跃事件并返回当前时间戳。
     * @param[in] timeoutMs 超时时间（毫秒），-1 表示无限等待。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @return 当前时间戳。
     * @note 忽略 EINTR 错误，记录其他错误。
     */
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

    /**
     * @brief 更新 Channel 的事件状态。
     * @param[in] channel 要更新的 Channel。
     * @note 添加或修改 pollfd 列表中的事件，忽略无事件 Channel。
     * @throw 如果不在事件循环线程中，抛出断言错误。
     */
    void updateChannel(Channel* channel) override;

    /**
     * @brief 从 poll 中移除 Channel。
     * @param[in] channel 要移除的 Channel。
     * @note 从 pollfd 列表和 Channel 映射中移除。
     * @throw 如果不在事件循环线程中或 Channel 状态不合法，抛出断言错误。
     */
    void removeChannel(Channel* channel) override;

private:
    /**
     * @typedef std::vector&lt;struct pollfd&gt;
     * @brief pollfd 列表类型，存储文件描述符及其事件。
     */
    using PollFDList = std::vector<pollfd>;

    /**
     * @brief 填充活跃的 Channel 列表。
     * @param[in] numEvents 触发的事件数量。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @note 根据 pollfd 的 revents 字段通知 Channel。
     */
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

private:
    PollFDList m_pollFDs; ///< 存储 pollfd 的列表
};
} // namespace zmuduo::net

#endif