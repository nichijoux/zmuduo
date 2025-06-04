// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_THREAD_H
#define ZMUDUO_BASE_THREAD_H

#include "zmuduo/base/nocopyable.h"
#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <unistd.h>

namespace zmuduo::thread {

/**
 * @class Thread
 * @brief 线程封装类（不可拷贝）
 *
 * Thread 封装了 std::thread，提供统一的线程创建、管理接口。<br/>
 * 支持设置线程名称、记录线程 ID，确保线程启动与 join 的正确性。
 *
 * @note 启动线程后，线程 ID 会通过信号量机制同步回主线程。
 */
class Thread : NoCopyable {
  public:
    /**
     * @brief 线程任务类型定义
     *
     * 表示一个无参无返回值的可调用对象。
     */
    using Task = std::function<void()>;

  public:
    /**
     * @brief 构造函数
     *
     * @param[in] task 要在线程中执行的任务
     * @param[in] name 线程名称（可选）
     *
     * @note 如果未提供名称，则自动生成 "Thread - N" 格式的默认名称。
     */
    explicit Thread(Task task, std::string name = std::string());

    /**
     * @brief 析构函数
     *
     * 若线程已启动但未 join，则自动 detach。
     */
    ~Thread();

    /**
     * @brief 启动线程
     *
     * 实际创建 std::thread 并异步执行任务。线程启动后会通过信号量
     * 通知主线程其 ID 获取完毕，避免竞态。
     *
     * @note 如果任务执行抛出异常，将记录日志。
     */
    void start();

    /**
     * @brief 阻塞等待线程结束（join）
     *
     * @note 必须在已启动、未 join 的前提下调用。
     */
    void join();

    /**
     * @brief 是否已经启动
     * @retval true 已启动
     * @retval false 未启动
     */
    bool isStarted() const {
        return m_started;
    }

    /**
     * @brief 是否已经 join
     * @retval true 已 join
     * @retval false 未 join
     */
    bool isJoined() const {
        return m_joined;
    }

    /**
     * @brief 获取线程 ID
     * @return 返回线程的 pid_t 类型 ID
     */
    pid_t getTid() const {
        return m_tid;
    }

    /**
     * @brief 获取线程名称
     * @return 返回线程名称的引用
     */
    const std::string& getName() const {
        return m_name;
    }

    /**
     * @brief 获取创建的线程总数
     * @return 返回已创建的线程数量
     */
    static int getNumCreated() {
        return S_NUM_CREATED;
    }

  private:
    /**
     * @brief 设置默认线程名称（如 "Thread - 1"）
     *
     * 如果未提供名称，将按线程创建序号自动命名。
     */
    void setDefaultName();

  private:
    bool                    m_started;      ///< 是否已启动
    bool                    m_joined;       ///< 是否已 join
    std::thread             m_thread;       ///< std::thread 对象
    pid_t                   m_tid;          ///< 系统级线程 ID
    Task                    m_task;         ///< 线程执行任务
    std::string             m_name;         ///< 线程名称
    static std::atomic<int> S_NUM_CREATED;  ///< 已创建线程数（全局静态）
};
}  // namespace zmuduo::thread
#endif