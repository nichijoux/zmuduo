#include "zmuduo/net/event_loop_thread_pool.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/event_loop_thread.h"
#include <cassert>
#include <utility>

namespace zmuduo::net {
EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, std::string name)
    : m_baseLoop(baseLoop),
      m_name(std::move(name)) {}

void EventLoopThreadPool::start(const ThreadInitCallback& callback) {
    // 启动线程池
    assert(!m_started);
    m_started = true;
    // 遍历子线程数量并创建subLoop
    for (int i = 0; i < m_numThreads; ++i) {
        // 创建EventLoopThread
        std::string name = m_name + std::to_string(i);
        auto*       t    = new EventLoopThread(callback, name);
        m_threads.emplace_back(std::unique_ptr<EventLoopThread>(t));
        // 启动线程,绑定一个eventLoop并返回该loop的地址
        m_subLoops.emplace_back(t->startLoop());
    }
    // 只有baseLoop且回调不为空
    if (m_numThreads == 0 && callback) {
        callback(m_baseLoop);
    }
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    m_baseLoop->assertInLoopThread();
    EventLoop* loop = m_baseLoop;
    if (!m_subLoops.empty()) {
        // 如果子loop不为空
        loop = m_subLoops[m_next];
        ++m_next;
        // 重置next
        if (static_cast<size_t>(m_next) >= m_subLoops.size()) {
            m_next = 0;
        }
    }
    return loop;
}

EventLoop* EventLoopThreadPool::getLoopForHash(const size_t hashCode) const {
    m_baseLoop->assertInLoopThread();
    EventLoop* loop = m_baseLoop;
    if (!m_subLoops.empty()) {
        // 如果子loop不为空，则根据hashCode获取对应的loop
        loop = m_subLoops[hashCode % m_subLoops.size()];
    }
    return loop;
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    m_baseLoop->assertInLoopThread();
    assert(m_started);

    if (m_subLoops.empty()) {
        return std::vector(1, m_baseLoop);
    }
    return m_subLoops;
}
} // namespace zmuduo::net