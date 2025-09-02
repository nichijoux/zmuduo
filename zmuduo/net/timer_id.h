// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_TIMER_ID_H
#define ZMUDUO_NET_TIMER_ID_H

#include "zmuduo/base/copyable.h"
#include <memory>

namespace zmuduo::net {
// Forward declaration
class Timer;

/**
 * @class TimerId
 * @brief 定时器标识类，用于唯一标识和管理定时器
 *
 * 该类提供定时器的唯一标识，包含定时器指针和序列号，
 * 用于在TimerQueue中精确识别和操作特定定时器。
 *
 * @note 继承自Copyable，表示可拷贝
 * @note 主要被TimerQueue类使用，用于取消定时器等操作
 *
 * @example
 *
 * @code
 * // 创建TimerId
 * Timer* timer = ...;
 * TimerId id1(timer, 1);
 *
 * // 默认构造
 * TimerId id2;
 * @endcode
 */
class TimerId : public Copyable {
public:
    /**
     * @brief 默认构造函数
     * @note 创建一个空的TimerId，timer指针为nullptr，序列号为0
     */
    TimerId() = default;

    /**
     * @brief 参数化构造函数
     * @param[in] timer 定时器指针
     * @param[in] sequence 定时器序列号
     * @note 序列号用于区分相同timer指针的不同实例
     */
    TimerId(const std::weak_ptr<Timer>& timer, int64_t sequence)
        : m_timer(timer),
          m_sequence(sequence) {}

    /**
     * @brief 默认析构函数
     */
    ~TimerId() = default;

    // 声明TimerQueue为友元类，允许其访问私有成员
    friend class TimerQueue;

private:
    std::weak_ptr<Timer> m_timer;        ///< 指向关联定时器的指针
    int64_t              m_sequence = 0; ///< 定时器序列号，用于唯一标识
};
} // namespace zmuduo::net

#endif