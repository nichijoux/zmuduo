// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TIMER_QUEUE_H
#define ZMUDUO_NET_TIMER_QUEUE_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/channel.h"
#include <set>
#include <tuple>
#include <vector>

namespace zmuduo::net {
class EventLoop;
class Timer;
class TimerId;

/**
 * @class TimerQueue
 * @brief 定时器队列，基于timerfd的高效定时器管理
 *
 * 使用Linux的timerfd机制实现的高效定时器队列，支持添加、取消定时器，
 * 并保证所有操作都在事件循环线程中执行，线程安全。
 *
 * @note 定时器按照到期时间排序，使用红黑树(set)存储保证高效查找
 * @note 继承自NoCopyable，不可拷贝
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * TimerQueue queue(&loop);
 *
 * // 添加一次性定时器
 * TimerId id = queue.addTimer([]{
 *     ZMUDUO_LOG_INFO << "Timer fired!";
 * }, Timestamp::Now() + 3.0, 0.0);
 *
 * // 添加重复定时器
 * TimerId repeatId = queue.addTimer([]{
 *     ZMUDUO_LOG_INFO << "Repeated timer!";
 * }, Timestamp::Now() + 1.0, 5.0);
 *
 * // 取消定时器
 * queue.cancel(repeatId);
 * @endcode
 */
class TimerQueue : NoCopyable {
  public:
    /**
     * @brief 构造函数
     * @param[in] loop 所属的事件循环对象
     * @note 会创建timerfd并设置对应的Channel
     * @note 自动启用Channel的读事件监听
     */
    explicit TimerQueue(EventLoop* loop);

    /**
     * @brief 析构函数
     * @note 关闭timerfd并移除对应的Channel
     */
    ~TimerQueue();

    /**
     * @brief 添加定时器
     * @param[in] cb 定时器回调函数
     * @param[in] when 定时器触发时间
     * @param[in] interval 重复间隔(秒)，0表示一次性定时器
     * @return TimerId 定时器标识符，可用于取消定时器
     * @note 实际添加操作会在事件循环线程中执行
     * @note 如果定时器是最早到期的，会立即重置timerfd
     */
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    /**
     * @brief 取消定时器
     * @param[in] timerId 要取消的定时器标识符
     * @note 实际取消操作会在事件循环线程中执行
     * @note 如果定时器正在执行回调，会延迟到回调结束后取消
     */
    void cancel(const TimerId& timerId);

  private:
    /// @brief 定时器条目类型(过期时间, Timer指针, 序列号)
    using Entry = std::tuple<Timestamp, std::shared_ptr<Timer>, int64_t>;
    /// @brief 定时器集合类型(按过期时间排序)
    using TimerSet = std::set<Entry>;

    /**
     * @brief 在事件循环线程中添加定时器
     * @param[in] timer 要添加的定时器
     * @note 必须在事件循环线程中调用
     */
    void addTimerInLoop(const std::shared_ptr<Timer>& timer);

    /**
     * @brief 在事件循环线程中取消定时器
     * @param[in] timerId 要取消的定时器标识符
     * @note 必须在事件循环线程中调用
     */
    void cancelInLoop(const TimerId& timerId);

    /**
     * @brief 处理timerfd可读事件
     * @note 读取timerfd并执行所有已到期的定时器回调
     */
    void handleRead();

    /**
     * @brief 获取所有已到期的定时器
     * @param[in] now 当前时间
     * @return 已到期的定时器列表
     */
    std::vector<Entry> getExpired(const Timestamp& now);

    /**
     * @brief 重置重复定时器
     * @param[in] expired 已到期的定时器列表
     * @param[in] now 当前时间
     * @note 会重新安排重复定时器的下一次触发时间
     */
    void reset(const std::vector<Entry>& expired, const Timestamp& now);

    /**
     * @brief 插入定时器到队列
     * @param[in] timer 要插入的定时器
     * @return bool 是否改变了最早到期时间
     * @note 如果插入的定时器是最早到期的，返回true
     */
    bool insert(const std::shared_ptr<Timer>& timer);

  private:
    EventLoop* m_eventLoop;             ///< 所属的事件循环
    const int  m_timerFD;               ///< 定时器文件描述符
    Channel    m_timerChannel;          ///< 定时器对应的Channel
    TimerSet   m_timers;                ///< 按过期时间排序的定时器集合
    TimerSet   m_cancelingTimers;       ///< 正在取消的定时器集合
    bool       m_callingExpiredTimers;  ///< 是否正在处理到期定时器
};
}  // namespace zmuduo::net

#endif