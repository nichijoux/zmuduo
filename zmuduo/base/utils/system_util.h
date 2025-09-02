// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_SYSTEM_UTIL_H
#define ZMUDUO_BASE_UTILS_SYSTEM_UTIL_H

#include <cstdint>
#include <sys/types.h>

/**
 * @namespace system_util
 * @brief 系统工具空间，提供进程、线程和时间相关的静态方法。
 *
 * system_util 封装了 POSIX 系统调用，用于获取进程/线程 ID、当前时间（毫秒和微秒）、线程睡眠等功能。
 * 所有方法均为静态，无需实例化，适合日志记录、性能分析和线程控制。
 *
 * @note system_util 不可拷贝或移动（继承自 NoCopyable 和 NoMoveable），所有方法线程安全。
 * @note GetTid 使用 thread_local 缓存线程 ID，减少系统调用开销。
 *
 * @example
 * @code
 * #include "zmuduo/base/utils/system_util.h"
 * #include <iostream>
 *
 * int main() {
 *     std::cout << "Process ID: " << zmuduo::utils::system_util::GetPid() << std::endl;
 *     std::cout << "Thread ID: " << zmuduo::utils::system_util::GetTid() << std::endl;
 *     uint64_t start = zmuduo::utils::system_util::GetCurrentMS();
 *     zmuduo::utils::system_util::Sleep(1);
 *     std::cout << "Elapsed: " << zmuduo::utils::system_util::GetCurrentMS() - start << " ms" <<
 * std::endl; return 0;
 * }
 * @endcode
 */
namespace zmuduo::utils::system_util {
/**
 * @brief 获取当前进程 ID。
 * @return 进程 ID（PID）。
 * @note 调用 POSIX getpid 系统调用，线程安全。
 */
pid_t GetPid();

/**
 * @brief 获取当前线程 ID。
 * @return 线程 ID（TID）。
 * @note 使用 thread_local 缓存 SYS_gettid
 * 结果，首次调用执行系统调用，后续直接返回缓存值，线程安全。
 */
pid_t GetTid();

/**
 * @brief 获取当前时间（毫秒）。
 * @return 自 Unix 纪元以来的毫秒数。
 * @note 使用 gettimeofday 获取高精度时间，精度约为毫秒，线程安全。
 */
uint64_t GetCurrentMS();

/**
 * @brief 获取当前时间（微秒）。
 * @return 自 Unix 纪元以来的微秒数。
 * @note 使用 gettimeofday 获取高精度时间，精度约为微秒，线程安全。
 */
uint64_t GetCurrentUS();

/**
 * @brief 使当前线程睡眠指定秒数。
 * @param[in] second 睡眠时间（秒）。
 * @note 调用 POSIX sleep 函数，可能被信号中断，线程安全。
 */
void Sleep(int64_t second);

/**
 * @brief 使当前线程睡眠指定微秒数。
 * @param[in] usec 睡眠时间（微秒）。
 * @note 调用 POSIX nanosleep 函数，精度较高，可能被信号中断，线程安全。
 */
void SleepUsec(int64_t usec);
} // namespace zmuduo::utils::system_util

#endif