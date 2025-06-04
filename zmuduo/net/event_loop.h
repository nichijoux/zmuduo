// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_EVENT_LOOP_H
#define ZMUDUO_NET_EVENT_LOOP_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/poller.h"
#include "zmuduo/net/timer_id.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace zmuduo::net {
class Channel;
class TimerQueue;
using TimerCallback = std::function<void()>;

/**
 * @class EventLoop
 * @brief 事件循环类，负责处理 IO 事件和定时器事件的核心组件。
 *
 * EventLoop 是网络库的核心，采用 Reactor 模式，通过 Poller 监听 IO 事件，通过 TimerQueue
 * 管理定时任务。<br/> 每个 EventLoop 绑定一个线程（One Loop Per Thread），通过 Channel
 * 处理文件描述符的事件。
 *
 * @note EventLoop 保证线程安全，跨线程操作通过 `runInLoop` 和 `queueInLoop` 异步执行。
 * @note 支持定时器任务（单次、延时、周期性）和事件驱动的回调机制。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * loop.runInLoop([]() { std::cout << "Run in loop thread\n"; });
 * loop.loop();
 * @endcode
 */
class EventLoop : NoCopyable {
  public:
    /**
     * @typedef std::function&lt;void()&gt;
     * @brief 定义回调函数类型，用于存储需要在事件循环中执行的任务。
     */
    using Functor = std::function<void()>;
    /**
     * @typedef std::vector&lt;Channel*&gt;
     * @brief 定义活跃 Channel 列表类型，用于存储触发事件的 Channel 指针。
     */
    using ChannelList = std::vector<Channel*>;

  public:
    /**
     * @brief 构造函数，初始化 EventLoop 实例。
     * @note 初始化 Poller、TimerQueue、wakeup 机制，并绑定当前线程。
     */
    EventLoop();

    /**
     * @brief 析构函数，清理 EventLoop 资源。
     * @note 确保关闭 wakeup 文件描述符并移除 wakeup Channel。
     */
    ~EventLoop();

    /**
     * @brief 断言当前线程为事件循环所属线程。
     * @warning 如果当前线程不是事件循环所属线程，打印致命日志错误并退出程序。
     */
    void assertInLoopThread() const;

    /**
     * @brief 启动事件循环，处理 IO 事件和定时器任务。
     * @note 该方法会阻塞当前线程，直到调用 quit 退出循环。
     */
    void loop();

    /**
     * @brief 退出事件循环。
     * @note 可在事件循环线程或非事件循环线程调用，跨线程调用会通过 wakeup 机制通知。
     */
    void quit();

    /**
     * @brief 获取最近一次 Poller 返回的时间戳。
     * @return 最近一次 Poller 返回的时间戳。
     */
    Timestamp getPollReturnTime() const {
        return m_pollReturnTime;
    }

    /**
     * @brief 在事件循环线程中立即执行回调函数。
     * @param[in] callback 需要执行的回调函数。
     * @note 如果当前线程是事件循环线程，立即执行；否则通过 queueInLoop 异步执行。
     */
    void runInLoop(Functor callback);

    /**
     * @brief 将回调函数加入队列，异步在事件循环线程中执行。
     * @param[in] callback 需要执行的回调函数。
     * @note 如果当前线程不是事件循环线程，会通过 wakeup 唤醒事件循环。
     */
    void queueInLoop(Functor callback);

    /**
     * @brief 获取待执行回调函数队列的大小。
     * @return 待执行回调函数的数量。
     */
    size_t getQueueSize();

    /**
     * @brief 在指定时间点执行定时器任务。
     * @param[in] time 执行任务的时间点。
     * @param[in] cb 定时器回调函数。
     * @return 定时器 ID，用于后续取消。
     */
    TimerId runAt(Timestamp time, TimerCallback cb);

    /**
     * @brief 在指定延时后执行定时器任务。
     * @param[in] delay 延时时间（秒）。
     * @param[in] cb 定时器回调函数。
     * @return 定时器 ID，用于后续取消。
     */
    TimerId runAfter(double delay, TimerCallback cb);

    /**
     * @brief 周期性执行定时器任务。
     * @param[in] interval 任务执行的周期（秒）。
     * @param[in] cb 定时器回调函数。
     * @return 定时器 ID，用于后续取消。
     */
    TimerId runEvery(double interval, TimerCallback cb);

    /**
     * @brief 取消指定的定时器任务。
     * @param[in] timerId 定时器 ID。
     */
    void cancel(const TimerId& timerId);

    /**
     * @brief 唤醒事件循环线程，处理待执行任务。
     * @note 通过写入 wakeup 文件描述符触发读事件。
     */
    void wakeup() const;

    /**
     * @brief 更新 Channel 的监听事件。
     * @param[in] channel 需要更新的 Channel 指针。
     * @note 必须在事件循环线程中调用，且 Channel 必须属于当前 EventLoop。
     */
    void updateChannel(Channel* channel);

    /**
     * @brief 移除指定的 Channel。
     * @param[in] channel 需要移除的 Channel 指针。
     * @note 必须在事件循环线程中调用，且 Channel 必须属于当前 EventLoop。
     */
    void removeChannel(Channel* channel);

    /**
     * @brief 检查指定 Channel 是否属于当前 EventLoop。
     * @param[in] channel 需要检查的 Channel 指针。
     * @return 如果 Channel 属于当前 EventLoop，返回 true；否则返回 false。
     */
    bool hasChannel(Channel* channel) const;

    /**
     * @brief 检查当前线程是否为事件循环所属线程。
     * @return 如果当前线程是事件循环所属线程，返回 true；否则返回 false。
     */
    bool isInLoopThread() const;

    /**
     * @brief 检查 EventLoop 指针是否为空。
     * @param[in] loop 需要检查的 EventLoop 指针。
     * @return 非空的 EventLoop 指针。
     * @warning 如果指针为空，打印致命日志错误并退出。
     */
    static EventLoop* checkNotNull(EventLoop* loop);

  private:
    /**
     * @brief 处理 wakeup 文件描述符的读事件。
     * @note 读取 wakeup 文件描述符的内容以清空事件。
     */
    void handleRead() const;

    /**
     * @brief 执行待处理的回调函数队列。
     * @note 确保线程安全，执行期间会清空回调队列。
     */
    void doPendingFunctors();

    std::atomic<bool>       m_looping;                 ///< 标记事件循环是否正在运行
    std::atomic<bool>       m_quit;                    ///< 标记是否需要退出事件循环
    std::atomic<bool>       m_eventHandling;           ///< 标记是否正在处理事件
    std::atomic<bool>       m_callingPendingFunctors;  ///< 标记是否正在执行待处理回调
    std::vector<Functor>    m_pendingFunctors;         ///< 待执行的回调函数队列
    std::mutex              m_mutex;                   ///< 保护回调队列的互斥锁
    int64_t                 m_iteration;               ///< 事件循环的迭代次数
    const pid_t             m_threadId;                ///< 事件循环所属线程 ID
    Timestamp               m_pollReturnTime;          ///< 最近一次 Poller 返回的时间戳
    std::unique_ptr<Poller> m_poller;                  ///< IO 事件监听器
    std::unique_ptr<TimerQueue> m_timerQueue;          ///< 定时器队列
    int                         m_wakeupFd;            ///< wakeup 机制的文件描述符
    std::unique_ptr<Channel>    m_wakeupChannel;       ///< wakeup 机制的 Channel
    ChannelList                 m_activeChannels;      ///< 活跃的 Channel 列表
    Channel*                    m_currentActiveChannel;  ///< 当前处理的活跃 Channel
};
}  // namespace zmuduo::net

#endif