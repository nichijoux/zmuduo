#include "timestamp.h"

#include <ctime>
#include <sys/time.h>

namespace zmuduo {
Timestamp::Timestamp(const Date& date)
    : m_microSecondsSinceEpoch(date.getMicroSecondsSinceEpoch()) {}

Timestamp Timestamp::FromUnixTime(time_t t, int microseconds) {
    return Timestamp(static_cast<int64_t>(t) * S_MICRO_SECONDS_PER_SECOND + microseconds);
}

Timestamp Timestamp::Now() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    return Timestamp(seconds * S_MICRO_SECONDS_PER_SECOND + tv.tv_usec);
}

std::string Timestamp::toString() const {
    // 1. 将微秒时间戳转换为秒级时间戳
    auto seconds      = static_cast<time_t>(m_microSecondsSinceEpoch / S_MICRO_SECONDS_PER_SECOND);
    int  microseconds = static_cast<int>(m_microSecondsSinceEpoch % S_MICRO_SECONDS_PER_SECOND);

    // 2. 转换为本地时间
    tm tm_time{};
    localtime_r(&seconds, &tm_time);  // 使用线程安全的 localtime_r

    // 3. 格式化输出（含微秒）
    char buffer[64] = {0};
    snprintf(buffer, sizeof(buffer), "%4d-%02d-%02d %02d:%02d:%02d.%06d", tm_time.tm_year + 1900,
             tm_time.tm_mon + 1, tm_time.tm_mday, tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec,
             microseconds);

    return buffer;
}

Date Date::Now() {
    timeval tv{};
    gettimeofday(&tv, nullptr);
    int64_t seconds = tv.tv_sec;
    return Date(seconds * Timestamp::S_MICRO_SECONDS_PER_SECOND + tv.tv_usec);
}

time_t Date::toDateEpoch() const {
    // 1. 将微秒时间戳转换为秒级时间戳
    auto seconds = static_cast<time_t>(m_microSecondsSinceEpoch / 1000000);

    // 2. 转换为本地时间
    tm tm_time{};
    localtime_r(&seconds, &tm_time);  // 使用线程安全的 localtime_r

    // 3. 重新计算只包含日期部分的时间戳
    tm_time.tm_hour = 0;
    tm_time.tm_min  = 0;
    tm_time.tm_sec  = 0;
    return mktime(&tm_time);
}

std::string Date::toString() const {
    // 1. 将微秒时间戳转换为秒级时间戳
    auto seconds = static_cast<time_t>(m_microSecondsSinceEpoch / 1000000);

    // 2. 转换为本地时间
    tm tm_time{};
    localtime_r(&seconds, &tm_time);  // 使用线程安全的 localtime_r

    // 3. 格式化输出（只包含日期）
    char buffer[32] = {0};
    snprintf(buffer, sizeof(buffer), "%4d-%02d-%02d", tm_time.tm_year + 1900, tm_time.tm_mon + 1,
             tm_time.tm_mday);

    return buffer;
}

}  // namespace zmuduo