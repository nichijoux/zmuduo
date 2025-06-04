// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_EVENT_LOOP_THREAD_POOL_H
#define ZMUDUO_NET_EVENT_LOOP_THREAD_POOL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/event_loop_thread.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace zmuduo::net {
/**
 * @class EventLoopThreadPool
 * @brief 管理一组 EventLoopThread 的线程池，用于多线程事件驱动模型。
 *
 * EventLoopThreadPool 维护一个主循环（baseLoop）和多个子循环（subLoops）。<br/>
 * 通过轮询或哈希分配 Channel 给子循环，实现负载均衡。<br/>
 * 适用于多线程 Reactor 模型，优化高并发场景下的 IO 处理。
 *
 * @note 主循环（baseLoop）运行在主线程，子循环运行在各自的 EventLoopThread 中。
 * @note 支持初始化回调，用于配置每个子循环。
 *
 * @example
 *
 * @code
 * EventLoopThreadPool pool(baseLoop, "ThreadPool");
 * pool.setThreadNum(4);
 * pool.start([](EventLoop* loop) {
 *     // 初始化回调
 * });
 * EventLoop* nextLoop = pool.getNextLoop();
 * @endcode
 */
class EventLoopThreadPool : NoCopyable {
  public:
    /**
     * @typedef std::shared_ptr&lt;EventLoopThreadPool&gt;
     * @brief 智能指针类型
     */
    using Ptr = std::shared_ptr<EventLoopThreadPool>;
    /**
     * @typedef std::function&lt;void(EventLoop*)&gt;
     * @brief 定义线程初始化回调函数类型，用于配置子循环。
     */
    using ThreadInitCallback = std::function<void(EventLoop*)>;

  public:
    /**
     * @brief 构造函数，初始化线程池。
     * @param[in] baseLoop 主事件循环指针。
     * @param[in] name 线程池名称。
     */
    EventLoopThreadPool(EventLoop* baseLoop, std::string name);

    /**
     * @brief 析构函数，默认实现，依赖智能指针自动清理。
     */
    ~EventLoopThreadPool() = default;

    /**
     * @brief 设置线程池的子线程数量。
     * @param[in] numThreads 子线程数量。
     */
    void setThreadNum(int numThreads) {
        m_numThreads = numThreads;
    }

    /**
     * @brief 启动线程池，创建并初始化子循环。
     * @param[in] callback 线程初始化回调函数，默认为空。
     * @note 必须在主循环线程中调用，且线程池未启动。
     * @throw 如果线程池已启动，抛出断言错误。
     */
    void start(const ThreadInitCallback& callback = ThreadInitCallback());

    /**
     * @brief 检查线程池是否已启动。
     * @return 如果线程池已启动，返回 true；否则返回 false。
     */
    bool isStarted() const {
        return m_started;
    }

    /**
     * @brief 获取线程池的名称。
     * @return 线程池的名称。
     */
    const std::string& getName() const {
        return m_name;
    }

    /**
     * @brief 获取下一个子循环（轮询分配）。
     * @return 下一个子循环的指针，如果没有子循环，返回主循环。
     * @note 使用轮询策略分配 Channel，确保负载均衡。
     * @throw 如果不在主循环线程中，抛出断言错误。
     */
    EventLoop* getNextLoop();

    /**
     * @brief 根据哈希值获取子循环。
     * @param[in] hashCode 哈希值，用于选择子循环。
     * @return 选中的子循环指针，如果没有子循环，返回主循环。
     * @note 使用哈希取模分配 Channel，适合固定分配场景。
     * @throw 如果不在主循环线程中，抛出断言错误。
     */
    EventLoop* getLoopForHash(size_t hashCode);

    /**
     * @brief 获取所有事件循环（主循环和子循环）。
     * @return 包含所有事件循环的向量。
     * @note 如果没有子循环，仅返回主循环。
     * @throw 如果线程池未启动，抛出断言错误。
     */
    std::vector<EventLoop*> getAllLoops();

    /**
     * @brief 获取主事件循环。
     * @return 主事件循环的指针。
     */
    const EventLoop* getBaseLoop() const {
        return m_baseLoop;
    }

  private:
    EventLoop*                                    m_baseLoop;    ///< 主事件循环
    bool                                          m_started;     ///< 线程池是否已启动
    std::string                                   m_name;        ///< 线程池名称
    int                                           m_numThreads;  ///< 子线程数量
    int                                           m_next;     ///< 下一次轮询的子循环索引
    std::vector<std::unique_ptr<EventLoopThread>> m_threads;  ///< 管理的 EventLoopThread 列表
    std::vector<EventLoop*>                       m_subLoops;  ///< 管理的子循环列表
};
}  // namespace zmuduo::net

#endif