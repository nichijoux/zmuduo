// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_MUTEX_H
#define ZMUDUO_BASE_MUTEX_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include <mutex>
#include <semaphore.h>
#include <shared_mutex>

namespace zmuduo {

/**
 * @class Semaphore
 * @brief 封装 POSIX 信号量的轻量级同步原语类
 *
 * 该类提供简单的计数信号量机制，用于线程间同步控制。<br/>
 * 支持阻塞式的 wait() 和释放信号量的 notify() 操作。
 *
 * @note 该类不可拷贝（继承自 NoCopyable）
 */
class Semaphore : NoCopyable {
  public:
    /**
     * @brief 构造函数
     *
     * 初始化一个指定初始值的信号量。
     *
     * @param[in] count 信号量的初始计数值，默认值为0
     *
     * @throw std::logic_error 如果 sem_init 调用失败
     */
    explicit Semaphore(unsigned int count = 0);

    /**
     * @brief 析构函数
     *
     * 释放底层信号量资源。
     */
    ~Semaphore();

    /**
     * @brief 获取信号量（阻塞）
     *
     * 若信号量为0，则阻塞等待；否则将信号量减一。
     *
     * @throw std::logic_error 如果 sem_wait 调用失败
     */
    void wait();

    /**
     * @brief 释放信号量
     *
     * 将信号量值加一，若有其他线程阻塞在wait上，则唤醒一个。
     *
     * @throw std::logic_error 如果 sem_post 调用失败
     */
    void notify();

  private:
    sem_t m_semaphore;  ///< POSIX 信号量对象
};

/**
 * @class ReadWriteLock
 * @brief 读写锁实现类，支持共享读和独占写
 *
 * 基于C++17的std::shared_mutex实现，提供多种RAII风格的锁守卫。<br/>
 * 支持手动锁定/解锁和尝试锁定操作。
 */
class ReadWriteLock {
  public:
    /**
     * @class ReadGuard
     * @brief 读锁守卫(RAII)，自动管理共享锁的生命周期
     */
    class ReadGuard : NoCopyable, NoMoveable {
      public:
        /**
         * @brief 构造函数，获取共享读锁
         * @param[in] mutex 要锁定的读写锁引用
         * @note 会阻塞直到获取锁成功
         */
        explicit ReadGuard(std::shared_mutex& mutex) : m_mutex(mutex) {
            m_mutex.lock_shared();
        }

        /**
         * @brief 析构函数，自动释放共享读锁
         */
        ~ReadGuard() {
            m_mutex.unlock_shared();
        }

      private:
        std::shared_mutex& m_mutex;  ///< 关联的读写锁引用
    };

    /**
     * @class WriteGuard
     * @brief 写锁守卫(RAII)，自动管理独占锁的生命周期
     */
    class WriteGuard : NoCopyable, NoMoveable {
      public:
        /**
         * @brief 构造函数，获取独占写锁
         * @param[in] mutex 要锁定的读写锁引用
         * @note 会阻塞直到获取锁成功
         */
        explicit WriteGuard(std::shared_mutex& mutex) : m_mutex(mutex) {
            m_mutex.lock();
        }

        /**
         * @brief 析构函数，自动释放独占写锁
         */
        ~WriteGuard() {
            m_mutex.unlock();
        }

      private:
        std::shared_mutex& m_mutex;  ///< 关联的读写锁引用
    };

    /**
     * @class TryReadGuard
     * @brief 尝试读锁守卫(RAII)，非阻塞获取共享锁
     */
    class TryReadGuard : NoCopyable, NoMoveable {
      public:
        /**
         * @brief 构造函数，尝试获取共享读锁
         * @param[in] mutex 要尝试锁定的读写锁引用
         * @note 不会阻塞，立即返回
         */
        explicit TryReadGuard(std::shared_mutex& mutex)
            : m_mutex(mutex), m_locked(m_mutex.try_lock_shared()) {}

        /**
         * @brief 析构函数，自动释放已获取的锁(如果获取成功)
         */
        ~TryReadGuard() {
            if (m_locked) {
                m_mutex.unlock_shared();
            }
        }

        /**
         * @brief 检查是否成功获取锁
         * @return bool 锁获取状态
         * @retval true 成功获取锁
         * @retval false 获取锁失败
         */
        bool isLocked() const noexcept {
            return m_locked;
        }

      private:
        std::shared_mutex& m_mutex;   ///< 关联的读写锁引用
        bool               m_locked;  ///< 锁获取状态标志
    };

    /**
     * @class TryWriteGuard
     * @brief 尝试写锁守卫(RAII)，非阻塞获取独占锁
     */
    class TryWriteGuard : NoCopyable, NoMoveable {
      public:
        /**
         * @brief 构造函数，尝试获取独占写锁
         * @param[in] mutex 要尝试锁定的读写锁引用
         * @note 不会阻塞，立即返回
         */
        explicit TryWriteGuard(std::shared_mutex& mutex)
            : m_mutex(mutex), m_locked(m_mutex.try_lock()) {}

        /**
         * @brief 析构函数，自动释放已获取的锁(如果获取成功)
         */
        ~TryWriteGuard() {
            if (m_locked) {
                m_mutex.unlock();
            }
        }

        /**
         * @brief 检查是否成功获取锁
         * @return bool 锁获取状态
         * @retval true 成功获取锁
         * @retval false 获取锁失败
         */
        bool isLocked() const noexcept {
            return m_locked;
        }

      private:
        std::shared_mutex& m_mutex;   ///< 关联的读写锁引用
        bool               m_locked;  ///< 锁获取状态标志
    };

    /**
     * @brief 创建并返回一个读锁守卫(RAII)
     * @return ReadGuard 读锁守卫对象
     * @note [[nodiscard]]确保返回值不会被忽略
     */
    [[nodiscard]] ReadGuard getReadGuard() {
        return ReadGuard(m_mutex);
    }

    /**
     * @brief 创建并返回一个写锁守卫(RAII)
     * @return WriteGuard 写锁守卫对象
     * @note [[nodiscard]]确保返回值不会被忽略
     */
    [[nodiscard]] WriteGuard getWriteGuard() {
        return WriteGuard(m_mutex);
    }

    /**
     * @brief 创建并返回一个尝试读锁守卫(RAII)
     * @return TryReadGuard 尝试读锁守卫对象
     * @note [[nodiscard]]确保返回值不会被忽略
     */
    [[nodiscard]] TryReadGuard getTryReadGuard() {
        return TryReadGuard(m_mutex);
    }

    /**
     * @brief 创建并返回一个尝试写锁守卫(RAII)
     * @return TryWriteGuard 尝试写锁守卫对象
     * @note [[nodiscard]]确保返回值不会被忽略
     */
    [[nodiscard]] TryWriteGuard getTryWriteGuard() {
        return TryWriteGuard(m_mutex);
    }

    /**
     * @brief 手动获取共享读锁
     * @note 会阻塞直到获取锁成功
     */
    void lockRead() {
        m_mutex.lock_shared();
    }

    /**
     * @brief 手动释放共享读锁
     * @pre 调用线程必须持有共享读锁
     */
    void unlockRead() {
        m_mutex.unlock_shared();
    }

    /**
     * @brief 手动获取独占写锁
     * @note 会阻塞直到获取锁成功
     */
    void lockWrite() {
        m_mutex.lock();
    }

    /**
     * @brief 手动释放独占写锁
     * @pre 调用线程必须持有独占写锁
     */
    void unlockWrite() {
        m_mutex.unlock();
    }

    /**
     * @brief 尝试获取共享读锁(非阻塞)
     * @return bool 锁获取结果
     * @retval true 成功获取锁
     * @retval false 获取锁失败
     */
    bool tryLockRead() {
        return m_mutex.try_lock_shared();
    }

    /**
     * @brief 尝试获取独占写锁(非阻塞)
     * @return bool 锁获取结果
     * @retval true 成功获取锁
     * @retval false 获取锁失败
     */
    bool tryLockWrite() {
        return m_mutex.try_lock();
    }

  private:
    std::shared_mutex m_mutex;  ///< 底层共享互斥量对象
};
}  // namespace zmuduo

#endif