#include "zmuduo/net/event_loop.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/system_util.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/poller.h"
#include "zmuduo/net/timer_queue.h"
#include <cassert>
#include <csignal>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace {
/**
 * 定义一个线程局部存储变量 t_loopInThisThread，用于保存当前线程的事件循环对象指针
 */
thread_local zmuduo::net::EventLoop* t_loopInThisThread = nullptr;

/**
 * poll等待的超时时间
 */
constexpr int POLL_TIMEMS = 10000;

/**
 * 创建一个事件文件描述符
 * @return 返回创建成功的事件文件描述符
 */
int createEventFD() {
    // 调用eventfd函数创建一个事件文件描述符
    // 参数0表示初始值为0
    // EFD_NONBLOCK表示以非阻塞模式打开文件描述符
    // EFD_CLOEXEC表示在执行exec类函数时自动关闭文件描述符
    const int fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    // 检查文件描述符是否创建成功
    // 如果fd小于0，表示创建失败
    if (fd < 0) {
        // 调用LOG_FATAL函数输出错误信息
        // "E_FAILED in eventfd"表示在调用eventfd时失败
        ZMUDUO_LOG_FMT_FATAL("E_FAILED in eventfd,errno %d", errno);
    }
    // 返回创建成功的文件描述符
    return fd;
}

#pragma GCC diagnostic ignored "-Wold-style-cast"
/**
 * @class IgnoreSigPipe
 * @brief 忽略SIGPIPE信号
 */
class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe _initObject;
} // namespace

namespace zmuduo::net {
using namespace utils::system_util;

EventLoop::EventLoop()
    : m_threadId(GetTid()),
      m_poller(Poller::newPoller(this)),
      m_timerQueue(std::make_unique<TimerQueue>(this)),
      m_wakeupFd(createEventFD()),
      m_wakeupChannel(std::make_unique<Channel>(this, m_wakeupFd)) {
    ZMUDUO_LOG_FMT_DEBUG("EventLoop created %p in thread %d", this, m_threadId);
    if (t_loopInThisThread) {
        ZMUDUO_LOG_FMT_FATAL("Another EventLoop %p Exists in this thread %d", t_loopInThisThread,
                             m_threadId);
    }
    t_loopInThisThread = this;
    // 设置m_wakeupChannel的读事件回调函数为handleRead
    m_wakeupChannel->setReadCallback([this](const Timestamp&) { handleRead(); });
    m_wakeupChannel->enableReading();
}

EventLoop::~EventLoop() {
    ZMUDUO_LOG_FMT_DEBUG("EventLoop %p of thread %d destructs in thread %d", this, m_threadId,
                         GetTid());
    m_wakeupChannel->disableAll();
    m_wakeupChannel->remove();
    close(m_wakeupFd);
    t_loopInThisThread = nullptr;
}

void EventLoop::assertInLoopThread() const {
    if (!isInLoopThread()) {
        ZMUDUO_LOG_FMT_FATAL(
            "EventLoop::assertInLoopThread - EventLoop %p was created in threadId %d, "
            "current thread id %d",
            this, m_threadId, GetTid());
    }
}

void EventLoop::loop() {
    assert(!m_looping);
    assertInLoopThread();
    m_looping = true;
    m_quit    = false;

    ZMUDUO_LOG_FMT_INFO("EventLoop %p start looping", this);

    while (!m_quit) {
        // 清除上次活跃的channels
        m_activeChannels.clear();
        // 利用poller监听事件并返回活跃的channel
        // 监听两类fd，一类是client的fd，一类是wakeupFd
        m_pollReturnTime = m_poller->poll(POLL_TIMEMS, &m_activeChannels);
        ++m_iteration;
        m_eventHandling = true;
        for (const auto channel : m_activeChannels) {
            // 设置当前活跃的channel
            m_currentActiveChannel = channel;
            m_currentActiveChannel->handleEvent(m_pollReturnTime);
        }
        m_currentActiveChannel = nullptr;
        m_eventHandling        = false;
        // 执行当前事件循环中需要处理的回调函数
        doPendingFunctors();
    }
    ZMUDUO_LOG_FMT_INFO("EventLoop %p stop looping", this);
    m_looping = false;
}

void EventLoop::quit() {
    // 退出事件循环
    m_quit = true;
    // 检查当前线程是否是事件循环所在的线程
    if (!isInLoopThread()) {
        // 在subLoop(workerThread)中调用mainLoop(IO)的quit
        // wakeup函数的作用是唤醒wakeupFD，使其立即处理退出请求
        wakeup();
    }
}

// 定义一个函数，用于在事件循环中执行一个回调函数
void EventLoop::runInLoop(Functor callback) {
    // 检查当前线程是否是事件循环所在的线程
    if (isInLoopThread()) {
        // 如果是事件循环所在的线程，则直接执行回调函数
        callback();
    } else {
        // 如果不是事件循环所在的线程，则将回调函数放入队列中，等待事件循环线程执行
        queueInLoop(std::move(callback));
    }
}

void EventLoop::queueInLoop(Functor callback) {
    {
        std::lock_guard lock(m_mutex);
        m_pendingFunctors.emplace_back(std::move(callback));
    }
    // 如果不是在当前loop线程则唤醒loop线程
    // 如果当前正在执行回调doPendingFunctors(),则通知回调可执行(防止阻塞在poll中,因为执行完doPendingFunctors()会继续poll)
    if (!isInLoopThread() || m_callingPendingFunctors) {
        wakeup();
    }
}

size_t EventLoop::getQueueSize() {
    std::lock_guard lock(m_mutex);
    return m_pendingFunctors.size();
}

TimerId EventLoop::runAt(const Timestamp time, TimerCallback cb) const {
    return m_timerQueue->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(const double delay, TimerCallback cb) const {
    const Timestamp time = Timestamp::Now() + delay;
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(const double interval, TimerCallback cb) const {
    return m_timerQueue->addTimer(std::move(cb), Timestamp::Now() + interval, interval);
}

void EventLoop::cancel(const TimerId& timerId) const {
    m_timerQueue->cancel(timerId);
}

void EventLoop::wakeup() const {
    // 定义一个64位无符号整数变量one，并赋值为1
    constexpr uint64_t one = 1;
    // 将变量one写入文件描述符m_wakeupFd
    // 检查写入的字节数是否等于sizeof(one)，即8字节
    if (const ssize_t n = write(m_wakeupFd, &one, sizeof(one)); n != sizeof(one)) {
        // 如果写入的字节数不等于8，则记录错误日志
        // LOG_ERROR是一个宏或函数，用于记录错误信息
        // 这里记录的信息是写入的字节数n不等于8
        ZMUDUO_LOG_FMT_ERROR("EventLoop::wakeup() writes %ld bytes instead of 8", n);
    }
}

void EventLoop::updateChannel(Channel* channel) const {
    assert(channel->getOwnerLoop() == this);
    assertInLoopThread();
    m_poller->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assert(channel->getOwnerLoop() == this);
    assertInLoopThread();
    if (m_eventHandling) {
        // 如果要移除的 channel 是当前正在处理的 channel
        // 或者这个 channel 不在当前活跃的 channel 列表中
        assert(m_currentActiveChannel == channel ||
            std::find(m_activeChannels.begin(), m_activeChannels.end(), channel) ==
            m_activeChannels.end());
    }
    m_poller->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) const {
    assert(channel->getOwnerLoop() == this);
    assertInLoopThread();
    return m_poller->hasChannel(channel);
}

/**
 * 定义一个名为 EventLoop 的类中的成员函数 isInLoopThread
 * 该函数用于检查当前线程是否是事件循环所属的线程
 * @return 如果当前线程是事件循环所属的线程，则返回true，否则返回false
 */
bool EventLoop::isInLoopThread() const {
    // m_threadId 是 EventLoop 类的一个成员变量，保存了事件循环所属的线程ID
    // SystemUtils::GetThreadId() 调用syscall，用于获取当前线程的ID
    return m_threadId == GetTid();
}

EventLoop* EventLoop::checkNotNull(EventLoop* loop) {
    if (loop == nullptr) {
        ZMUDUO_LOG_FMT_FATAL("eventLoop is null!");
    }
    return loop;
}

void EventLoop::handleRead() const {
    uint64_t one = 1;
    if (const auto n = read(m_wakeupFd, &one, sizeof(one)); n != sizeof(one)) {
        ZMUDUO_LOG_FMT_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8", n);
    }
}

void EventLoop::doPendingFunctors() {
    // 定义一个局部变量functors，用于存储待处理的函数对象
    std::vector<Functor> functors;
    // 设置标志位，表示正在调用待处理的函数对象
    m_callingPendingFunctors = true;
    {
        // 创建一个互斥锁的守护对象，用于保护m_pendingFunctors的访问
        std::lock_guard lock(m_mutex);
        // 交换functors和m_pendingFunctors，将m_pendingFunctors中的函数对象转移到functors中
        functors.swap(m_pendingFunctors);
    }
    // 遍历functors中的所有回调并执行
    for (auto& functor : functors) {
        functor();
    }
    // 重置标志位，表示调用待处理的函数对象结束
    m_callingPendingFunctors = false;
}
} // namespace zmuduo::net