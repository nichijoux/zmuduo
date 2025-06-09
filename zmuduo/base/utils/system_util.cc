#include "zmuduo/base/utils/system_util.h"
#include "zmuduo/base/marco.h"
#include <csignal>
#include <sys/syscall.h>
#include <sys/time.h>
#include <zmuduo/base/timestamp.h>

namespace zmuduo::utils {
static thread_local pid_t tid = 0;

pid_t SystemUtil::GetPid() {
    return getpid();
}

pid_t SystemUtil::GetTid() {
    if (ZMUDUO_UNLIKELY(tid == 0)) {
        tid = static_cast<pid_t>(syscall(SYS_gettid));
    }
    return tid;
}

// 毫秒 ms
uint64_t SystemUtil::GetCurrentMS() {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

// 微秒 µs
uint64_t SystemUtil::GetCurrentUS() {
    struct timeval tv {};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

void SystemUtil::Sleep(int64_t second) {
    ::sleep(second);
}

void SystemUtil::SleepUsec(int64_t usec) {
    struct timespec ts {};
    ts.tv_sec  = static_cast<time_t>(usec / Timestamp::S_MICRO_SECONDS_PER_SECOND);
    ts.tv_nsec = static_cast<long>(usec % Timestamp::S_MICRO_SECONDS_PER_SECOND * 1000);
    ::nanosleep(&ts, nullptr);
}

}  // namespace zmuduo::utils