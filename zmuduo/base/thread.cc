#include "thread.h"
#include "logger.h"
#include "mutex.h"
#include "zmuduo/base/utils/system_util.h"
#include <cassert>
#include <utility>

namespace zmuduo::thread {
std::atomic<int> Thread::S_NUM_CREATED(0);

void Thread::setDefaultName() {
    int num = ++S_NUM_CREATED;
    if (m_name.empty()) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "Thread - %d", num);
        m_name = buffer;
    }
}

Thread::Thread(Task task, std::string name)
    : m_started(false),
      m_joined(false),
      m_thread(),
      m_tid(0),
      m_task(std::move(task)),
      m_name(std::move(name)) {
    setDefaultName();
}

Thread::~Thread() {
    if (m_started && !m_joined) {
        m_thread.detach();
    }
}

void Thread::start() {
    assert(!m_started);

    m_started = true;
    // 启动线程执行任务,通过信号量设置线程的id
    Semaphore semaphore;
    m_thread = std::thread([&]() {
        try {
            // 获取线程的id
            m_tid = utils::system_util::GetTid();
            semaphore.notify();
            // 执行任务
            m_task();
        } catch (const std::exception& e) {
            ZMUDUO_LOG_FMT_ERROR("exception caught in %s,reason: %s", m_name.c_str(), e.what());
        } catch (...) { ZMUDUO_LOG_FMT_ERROR("unknown exception caught in %s", m_name.c_str()); }
    });

    // 等待线程获取到id
    semaphore.wait();
}

void Thread::join() {
    assert(m_started && !m_joined);
    m_joined = true;
    m_thread.join();
}

}  // namespace zmuduo::thread