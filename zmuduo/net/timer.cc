#include "zmuduo/net/timer.h"

#include <utility>

namespace zmuduo::net {
std::atomic<int64_t> Timer::S_NUM_CREATED = 0;

Timer::Timer(TimerCallback callback, Timestamp when, double interval)
    : m_callback(std::move(callback)),
      m_expiration(std::move(when)),
      m_interval(interval),
      m_repeat(interval > 0.0),
      m_sequence(S_NUM_CREATED++) {}

void Timer::restart(const Timestamp& now) {
    if (m_repeat) {
        m_expiration = now + m_interval;
    } else {
        m_expiration = Timestamp::Invalid();
    }
}

void Timer::run() const {
    m_callback();
}
} // namespace zmuduo::net