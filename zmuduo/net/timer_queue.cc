#include "zmuduo/net/timer_queue.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/timer.h"
#include "zmuduo/net/timer_id.h"
#include <cassert>
#include <cstring>
#include <sys/timerfd.h>
#include <unistd.h>
#include <utility>

namespace zmuduo::net {
/// @brief 创建一个定时器文件描述符
/// @return 创建的定时器文件描述符
int createTimerFD() {
    // 使用timerfd_create函数创建一个定时器文件描述符
    // CLOCK_MONOTONIC表示使用单调递增的时钟，不受系统时间更改的影响
    // TFD_NONBLOCK设置文件描述符为非阻塞模式
    // TFD_CLOEXEC设置文件描述符在执行exec类函数时自动关闭
    const int timerFD = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timerFD < 0) {
        ZMUDUO_LOG_FMT_FATAL("E_FAILED in timerfd_create");
    }
    return timerFD;
}

// @brief 计算从当前时间到指定时间戳之间的时间间隔，并返回一个timespec结构体表示的时间
// @param when - 指定的时间戳
// @return 时间间隔的结构体
timespec howMuchTimeFromNow(const Timestamp& when) {
    // 计算从当前时间到指定时间戳之间的微秒数差值
    int64_t microseconds =
        when.getMicroSecondsSinceEpoch() - Timestamp::Now().getMicroSecondsSinceEpoch();
    // 如果计算出的微秒数小于100，则将其设置为100，确保最小时间间隔为100微秒
    if (microseconds < 100) {
        microseconds = 100;
    }
    // 创建一个timespec结构体用于存储时间间隔
    timespec ts{};
    // 将微秒数转换为秒数
    ts.tv_sec = microseconds / Timestamp::S_MICRO_SECONDS_PER_SECOND;
    // 计算剩余的微秒数，并将其转换为纳秒数
    ts.tv_nsec = microseconds % Timestamp::S_MICRO_SECONDS_PER_SECOND * 1000;
    return ts;
}

void readTimerFD(const int timerFD, const Timestamp& now) {
    uint64_t      data;
    const ssize_t n = ::read(timerFD, &data, sizeof(data));
    ZMUDUO_LOG_FMT_INFO("readTimerFD %ld at %s", data, now.toString().c_str());
    if (n != sizeof(data)) {
        ZMUDUO_LOG_FMT_ERROR("readTimerFD reads %ld bytes instead of 8", n);
    }
}

/// @brief 重置定时器文件描述符（timerfd）的到期时间
/// @param timerFD 定时器文件描述符
/// @param expiration 定时器的到期时间戳
void resetTimerFD(const int timerFD, const Timestamp& expiration) {
    // 定义两个itimerspec结构体，用于设置和获取定时器的时间
    itimerspec newValue{};
    itimerspec oldValue{};
    // 初始化
    bzero(&newValue, sizeof(newValue));
    bzero(&oldValue, sizeof(oldValue));
    // 计算从现在到到期时间的间隔，并设置到newValue的it_value字段
    newValue.it_value = howMuchTimeFromNow(expiration);
    // 调用timerfd_settime函数设置定时器的新到期时间
    // 第一个参数是定时器文件描述符
    // 第二个参数为0，表示相对时间
    // 第三个参数是指向新时间值的指针
    // 第四个参数是指向旧时间值的指针，用于保存设置前的定时器时间
    // 检查timerfd_settime函数的返回值，如果不为0，表示设置失败
    if (const int ret = ::timerfd_settime(timerFD, 0, &newValue, &oldValue); ret != 0) {
        // 如果设置失败，输出错误日志
        ZMUDUO_LOG_FMT_ERROR("timerfd_settime failed the errno is %d", errno);
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : m_eventLoop(loop),
      m_timerFD(createTimerFD()),
      m_timerChannel(loop, m_timerFD),
      m_callingExpiredTimers(false) {
    // 注册读取事件回调
    m_timerChannel.setReadCallback([this](const Timestamp&) { handleRead(); });
    m_timerChannel.enableReading();
}

TimerQueue::~TimerQueue() {
    m_timerChannel.disableAll();
    m_timerChannel.remove();
    ::close(m_timerFD);
}

TimerId TimerQueue::addTimer(TimerCallback cb, const Timestamp when, const double interval) {
    std::shared_ptr<Timer> timer(new Timer(std::move(cb), when, interval));
    m_eventLoop->runInLoop([this, &timer] { addTimerInLoop(timer); });
    return {timer, timer->getSequence()};
}

void TimerQueue::cancel(const TimerId& timerId) {
    m_eventLoop->runInLoop([this, timerId] { cancelInLoop(timerId); });
}

void TimerQueue::addTimerInLoop(const std::shared_ptr<Timer>& timer) {
    m_eventLoop->assertInLoopThread();
    if (insert(timer)) {
        resetTimerFD(m_timerFD, timer->getExpiration());
    }
}

void TimerQueue::cancelInLoop(const TimerId& timerId) {
    m_eventLoop->assertInLoopThread();
    // 提升为强指针
    auto timer = timerId.m_timer.lock();
    if (!timer) {
        return;
    }
    const Entry entry(timer->getExpiration(), timer, timerId.m_sequence);
    if (const auto it = m_timers.find(entry); it != m_timers.end()) {
        // timers中存在当前定时器
        const size_t n = m_timers.erase(entry);
        assert(n == 1);
    } else if (m_callingExpiredTimers) {
        m_cancelingTimers.insert(entry);
    }
}

void TimerQueue::handleRead() {
    // 确保当前线程是事件循环所在的线程
    m_eventLoop->assertInLoopThread();
    const auto now = Timestamp::Now();
    // 读取定时器文件描述符，结束EPOLLIN动作
    readTimerFD(m_timerFD, now);
    // 获取所有已到期的定时器
    const auto expired = getExpired(now);

    // 标记正在调用已到期的定时器
    m_callingExpiredTimers = true;
    // 清空正在取消的定时器集合
    m_cancelingTimers.clear();
    // 遍历所有已到期的定时器，并执行其回调函数
    for (const auto& entry : expired) {
        std::get<Timer::Ptr>(entry)->run();
    }
    // 取消标记正在调用已到期的定时器
    m_callingExpiredTimers = false;
    // 重置已到期的定时器，重新设置其到期时间
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(const Timestamp& now) {
    // 存储已经过期的定时器条目
    std::vector<Entry> expired;
    const Entry        sentry(now, nullptr, INT64_MIN);
    // 使用lower_bound找到第一个不小于哨兵条目的位置(只比较时间)
    const auto end = m_timers.lower_bound(sentry);
    // 断言检查，确保end指向的是m_timers的末尾，或者当前时间小于end指向的条目的时间戳
    assert(end == m_timers.end() || now < std::get<Timestamp>(*end));
    // 将m_timers中从begin到end之前的所有条目复制到expired向量中
    std::copy(m_timers.begin(), end, back_inserter(expired));
    // 从m_timers中删除从begin到end之前的所有条目
    m_timers.erase(m_timers.begin(), end);
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, const Timestamp& now) {
    // 定义一个时间戳变量，用于存储下一个到期时间
    Timestamp nextExpire;
    // 遍历已到期的定时器列表
    for (const auto& entry : expired) {
        // 从entry中获取定时器指针
        // 检查定时器是否是重复定时器且不在取消列表中
        if (const auto& timer = std::get<Timer::Ptr>(entry);
            timer->isRepeat() && m_cancelingTimers.find(entry) == m_cancelingTimers.end()) {
            // 重启定时器
            timer->restart(now);
            // 将定时器重新插入定时器队列
            insert(timer);
        }
    }

    // 如果定时器队列不为空，获取下一个定时器的到期时间
    if (!m_timers.empty()) {
        nextExpire = std::get<Timer::Ptr>(*m_timers.begin())->getExpiration();
    }

    // 如果下一个到期时间有效，重置定时器文件描述符
    if (nextExpire.isValid()) {
        resetTimerFD(m_timerFD, nextExpire);
    }
}

bool TimerQueue::insert(const std::shared_ptr<Timer>& timer) {
    m_eventLoop->assertInLoopThread();
    // 标志变量，用于指示是否更新了最早到期时间
    bool earliestChanged = false;
    // 获取需要插入的定时器的到期时间
    Timestamp when = timer->getExpiration();
    // 获取m_timers的迭代器起始位置
    // 如果m_timers为空或者新定时器的到期时间早于当前最早到期的时间
    if (const auto it = m_timers.begin(); it == m_timers.end() || when < std::get<Timestamp>(*it)) {
        // 更新最早到期时间
        earliestChanged = true;
    }
    // 插入新的定时器到m_timers中
    const auto [fst, snd] = m_timers.insert(Entry(when, timer, timer->getSequence()));
    assert(snd);

    return earliestChanged;
}
} // namespace zmuduo::net