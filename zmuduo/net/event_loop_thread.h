// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_EVENT_LOOP_THREAD_H
#define ZMUDUO_NET_EVENT_LOOP_THREAD_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/thread.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

namespace zmuduo::net {
class EventLoop;

/**
 * @class EventLoopThread
 * @brief 封装一个线程和一个独立的事件循环，用于单线程事件驱动模型。
 *
 * EventLoopThread 创建一个新线程，并在该线程中运行一个独立的 EventLoop。<br/>
 * 支持在事件循环启动前执行初始化回调，用于配置 EventLoop。
 *
 * @note 每个 `EventLoopThread` 绑定一个线程和一个 `EventLoop`，遵循 One Loop Per Thread 原则。
 * @note 线程安全通过 `std::mutex` 和 `std::condition_variable` 保证。
 *
 * @example
 *
 * @code
 * EventLoopThread thread([](EventLoop* loop) {
 *     // 初始化回调
 * }, "WorkerThread");
 * EventLoop* loop = thread.startLoop();
 * @endcode
 */
class EventLoopThread : NoCopyable {
  public:
    /**
     * @typedef std::function&lt;void(EventLoop*)&gt;
     * @brief 定义线程初始化回调函数类型，用于在事件循环启动前配置。
     */
    using ThreadInitCallback = std::function<void(EventLoop*)>;

  public:
    /**
     * @brief 构造函数，初始化 EventLoopThread 实例。
     * @param[in] callback 线程初始化回调函数，默认为空。
     * @param[in] name 线程名称，默认为空字符串。
     */
    explicit EventLoopThread(ThreadInitCallback callback = ThreadInitCallback(),
                             const std::string& name     = std::string());

    /**
     * @brief 析构函数，清理线程和事件循环资源。
     * @note 如果事件循环仍在运行，会调用 quit 并等待线程结束。
     */
    ~EventLoopThread();

    /**
     * @brief 启动线程并返回事件循环指针。
     * @return 运行在子线程中的 `EventLoop` 指针。
     * @note 阻塞调用，直到事件循环初始化完成。
     * @throw 如果线程已启动，抛出断言错误。
     */
    EventLoop* startLoop();

  private:
    /**
     * @brief 线程函数，创建并运行事件循环。
     * @note 在新线程中执行，初始化 `EventLoop` 并进入循环。
     */
    void threadFunction();

  private:
    EventLoop*              m_loop;          ///< 运行在子线程中的事件循环指针
    zmuduo::thread::Thread  m_thread;        ///< 子线程对象
    std::mutex              m_mutex;         ///< 保护事件循环初始化的互斥锁
    std::condition_variable m_condition;     ///< 用于等待事件循环初始化的条件变量
    ThreadInitCallback      m_initCallback;  ///< 线程初始化回调函数
};
}  // namespace zmuduo::net

#endif