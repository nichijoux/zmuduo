// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_SELECT_POLLER_H
#define ZMUDUO_SELECT_POLLER_H
#include "zmuduo/net/poller.h"
#include <tuple>

namespace zmuduo::net {
/**
 * @class SelectPoller
 * @brief 基于 POSIX select 的 I/O 多路复用器，管理事件循环中的 Channel。
 *
 * SelectPoller 是 Reactor 模型中的核心组件，继承自 Poller 基类，使用 POSIX select 系统调用实现事件轮询。<br/>
 * 负责管理文件描述符的注册、更新和移除，轮询活跃事件，并通知对应的 Channel 处理读写或异常事件。<br/>
 * 使用 std::vector<std::tuple<int, uint32_t>> 维护文件描述符及其事件，配合 fd_set 实现 select 调用。
 *
 * @note 所有方法必须在所属 EventLoop 线程中调用，以确保线程安全。
 * @note 受限于 select 的文件描述符上限（通常为 1024），适合小型连接场景，性能低于 EpollPoller 和 PollPoller。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * SelectPoller poller(&loop);
 * Channel channel(&loop, socketFD);
 * channel.setReadCallback([] { std::cout << "Read event" << std::endl; });
 * channel.enableReading();
 * poller.updateChannel(&channel);
 * loop.loop();
 * @endcode
 */
class SelectPoller : public Poller {
  public:
    /**
     * @brief 构造函数，初始化 SelectPoller。
     * @param[in] eventLoop 所属的事件循环。
     */
    explicit SelectPoller(EventLoop* eventLoop) : Poller(eventLoop) {}

    /**
     * @brief 析构函数，清理资源。
     * @note 默认析构函数，自动释放文件描述符列表。
     */
    ~SelectPoller() override = default;

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
     * @note 添加或修改文件描述符的事件，忽略无事件 Channel。
     * @throw 如果不在事件循环线程中，抛出断言错误。
     */
    void updateChannel(Channel* channel) override;

    /**
     * @brief 从 select 中移除 Channel。
     * @param[in] channel 要移除的 Channel。
     * @note 从文件描述符列表和 Channel 映射中移除。
     * @throw 如果不在事件循环线程中或 Channel 状态不合法，抛出断言错误。
     */
    void removeChannel(Channel* channel) override;

  private:
    /**
     * @typedef std::vector&lt;std::tuple&lt;int, uint32_t&gt;&gt;
     * @brief 文件描述符及其事件列表类型，存储 fd 和事件。
     */
    using SelectFdList = std::vector<std::tuple<int, uint32_t>>;

    /**
     * @brief 填充活跃的 Channel 列表。
     * @param[in] numEvents 触发的事件数量。
     * @param[out] activeChannels 存储触发事件的 Channel 列表。
     * @param[in,out] readSet 可读文件描述符集合。
     * @param[in,out] writeSet 可写文件描述符集合。
     * @param[in,out] exceptSet 异常文件描述符集合。
     * @note 根据 fd_set 的状态通知 Channel。
     */
    void fillActiveChannels(int numEvents,
                            ChannelList* activeChannels,
                            fd_set& readSet,
                            fd_set& writeSet,
                            fd_set& exceptSet) const;

  private:
    SelectFdList m_selectFDs; ///< 存储文件描述符及其事件的列表
};
}  // namespace zmuduo::net

#endif