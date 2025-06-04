// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TIMER_H
#define ZMUDUO_NET_TIMER_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/callbacks.h"
#include <atomic>

namespace zmuduo::net {
/**
 * @class Timer
 * @brief 定时器类，用于在指定时间点执行回调函数
 *
 * 提供一次性定时和重复定时功能，内部使用原子计数器保证定时器序列号的唯一性。<br/>
 * 实际定时需要依赖TimerQueue
 * @note 该类不可拷贝(继承自NoCopyable)
 *
 * @example
 *
 * @code
 * Timer timer([]{
 *     ZMUDUO_LOG_INFO << "Timer fired!";
 * }, Timestamp::Now() + 3.0, 0.0);  // 3秒后执行，不重复
 *
 * Timer timer([]{
 *     ZMUDUO_LOG_INFO << "Repeated timer fired!";
 * }, Timestamp::Now() + 1.0, 5.0);  // 1秒后首次执行，之后每5秒执行一次
 * @endcode
 */
class Timer : NoCopyable {
  public:
    /**
     * @brief 构造函数
     * @param[in] callback 定时器回调函数
     * @param[in] when 定时器触发时间点
     * @param[in] interval 重复间隔(秒)，0表示一次性定时器
     * @note 如果interval>0，定时器会自动重新安排为重复定时器
     */
    Timer(TimerCallback callback, Timestamp when, double interval);

    /**
     * @brief 默认析构函数
     */
    ~Timer() = default;

    /**
     * @brief 重新启动定时器
     * @param[in] now 当前时间
     * @note 对于重复定时器，会根据interval重新计算过期时间
     * @note 对于一次性定时器，会将过期时间设为无效
     */
    void restart(const Timestamp& now);

    /**
     * @brief 执行定时器回调函数
     * @note 该方法只是简单地调用回调函数，不处理定时器状态
     */
    void run() const;

    /**
     * @brief 获取定时器过期时间
     * @return Timestamp 定时器将触发的时间点
     */
    Timestamp getExpiration() const {
        return m_expiration;
    }

    /**
     * @brief 检查是否是重复定时器
     * @return bool 定时器类型
     * @retval true 重复定时器
     * @retval false 一次性定时器
     */
    bool isRepeat() const {
        return m_repeat;
    }

    /**
     * @brief 获取定时器序列号
     * @return int64_t 定时器唯一序列号
     * @note 序列号由静态原子计数器生成，保证唯一性
     */
    int64_t getSequence() const {
        return m_sequence;
    }

  private:
    static std::atomic<int64_t> S_NUM_CREATED;  ///< 定时器创建计数器(原子变量)
  private:
    const TimerCallback m_callback;    ///< 定时器触发时执行的回调函数
    Timestamp           m_expiration;  ///< 定时器触发的时间点
    const double        m_interval;    ///< 重复间隔(秒)，0表示不重复
    const bool          m_repeat;      ///< 是否重复执行的标志
    const int64_t       m_sequence;    ///< 定时器唯一序列号
};
}  // namespace zmuduo::net

#endif