#include "zmuduo/base/utils/system_util.h"
#include "zmuduo/base/marco.h"
#include <csignal>
#include <sys/syscall.h>
#include <sys/time.h>
#include <zmuduo/base/timestamp.h>

namespace zmuduo::utils::system_util {
static thread_local pid_t tid = 0;

pid_t GetPid() {
    return getpid();
}

pid_t GetTid() {
    if (ZMUDUO_UNLIKELY(tid == 0)) {
        tid = static_cast<pid_t>(syscall(SYS_gettid));
    }
    return tid;
}

// 毫秒 ms
uint64_t GetCurrentMS() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000ul + tv.tv_usec / 1000;
}

// 微秒 µs
uint64_t GetCurrentUS() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * 1000 * 1000ul + tv.tv_usec;
}

void Sleep(const int64_t second) {
    ::sleep(second);
}

void SleepUsec(const int64_t usec) {
    timespec ts{};
    ts.tv_sec  = usec / Timestamp::S_MICRO_SECONDS_PER_SECOND;
    ts.tv_nsec = usec % Timestamp::S_MICRO_SECONDS_PER_SECOND * 1000;
    ::nanosleep(&ts, nullptr);
}
} // namespace zmuduo::utils::system_util