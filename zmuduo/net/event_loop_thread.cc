#include "zmuduo/net/event_loop_thread.h"
#include "zmuduo/net/event_loop.h"
#include <cassert>
#include <utility>

namespace zmuduo::net {
void EventLoopThread::threadFunction() {
    EventLoop loop;
    // 初始化的回调
    if (m_initCallback) {
        m_initCallback(&loop);
    }
    // 初始化eventLoop
    {
        std::unique_lock lock(m_mutex);
        m_loop = &loop;
        m_condition.notify_one();
    }
    // 开启事件循环
    m_loop->loop();
    // 结束事件循环
    std::unique_lock lock(m_mutex);
    m_loop = nullptr;
}

EventLoopThread::EventLoopThread(ThreadInitCallback callback, const std::string& name)
    : m_loop(nullptr),
      m_thread([this] { threadFunction(); }, name),
      m_initCallback(std::move(callback)) {}

EventLoopThread::~EventLoopThread() {
    if (m_loop != nullptr) {
        // 退出事件循环
        m_loop->quit();
        // 等待子线程结束
        m_thread.join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    assert(!m_thread.isStarted());
    // 启动线程
    m_thread.start();
    {
        std::unique_lock lock(m_mutex);
        while (m_loop == nullptr) {
            m_condition.wait(lock);
        }
    }
    return m_loop;
}
} // namespace zmuduo::net