// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_TIMESTAMP_H
#define ZMUDUO_BASE_TIMESTAMP_H

#include "zmuduo/base/copyable.h"
#include <string>

namespace zmuduo {
class Date;

/**
 * @class Timestamp
 * @brief 微秒精度的时间戳类
 *
 * 表示从 Unix 纪元（1970-01-01 00:00:00 UTC）起的微秒数。<br/>
 * 可用于记录高精度时间、日志时间戳、延迟计算等。
 */
class Timestamp : public Copyable {
public:
    static constexpr int S_MICRO_SECONDS_PER_SECOND = 1000000; ///< 每秒包含的微秒数

public:
    /**
     * @brief 默认构造函数，生成无效时间戳
     */
    Timestamp()
        : m_microSecondsSinceEpoch(0) {}

    /**
     * @brief 构造函数，使用日期
     * @param[in] date 日期
     */
    explicit Timestamp(const Date& date);

    /**
     * @brief 构造函数，使用指定微秒值
     * @param[in] microSecondsSinceEpoch 自 Unix 纪元起的微秒数
     */
    explicit Timestamp(const int64_t microSecondsSinceEpoch)
        : m_microSecondsSinceEpoch(microSecondsSinceEpoch) {}

    /**
     * @brief 由 Unix 秒级时间创建 Timestamp（微秒为0）
     * @param[in] t 秒级时间戳
     * @return 转换后的 Timestamp 对象
     */
    static Timestamp FromUnixTime(const time_t t) {
        return FromUnixTime(t, 0);
    }

    /**
     * @brief 由 Unix 时间创建 Timestamp（带微秒）
     * @param[in] t 秒级时间
     * @param[in] microseconds 微秒部分
     * @return 转换后的 Timestamp 对象
     */
    static Timestamp FromUnixTime(time_t t, int microseconds);

    /**
     * @brief 获取当前时间戳（微秒级）
     * @return 当前时间的 Timestamp 对象
     */
    static Timestamp Now();

    /**
     * @brief 生成一个无效的时间戳
     * @return 无效 Timestamp（时间值为0）
     */
    static Timestamp Invalid() {
        return {};
    }

    /**
     * @brief 获取微秒时间戳
     * @return 自 Unix 纪元以来的微秒数
     */
    int64_t getMicroSecondsSinceEpoch() const {
        return m_microSecondsSinceEpoch;
    }

    /**
     * @brief 获取秒级时间戳
     * @return 自 Unix 纪元以来的秒数
     */
    time_t getSecondsSinceEpoch() const {
        return m_microSecondsSinceEpoch / S_MICRO_SECONDS_PER_SECOND;
    }

    /**
     * @brief 判断时间戳是否合法
     * @retval true 有效（大于0）
     * @retval false 无效
     */
    bool isValid() const {
        return m_microSecondsSinceEpoch > 0;
    }

    /**
     * @brief 转换为字符串，格式为 yyyy-MM-dd HH:mm:ss.ffffff
     * @return 格式化时间字符串
     */
    std::string toString() const;

    /**
     * @brief 与另一个时间戳交换值
     * @param[in,out] other 另一个时间戳
     */
    void swap(Timestamp& other) noexcept {
        std::swap(m_microSecondsSinceEpoch, other.m_microSecondsSinceEpoch);
    }

    /**
     * @brief 加上指定秒数后的时间戳
     * @param[in] seconds 要增加的秒数（可为小数）
     * @return 新的时间戳
     */
    Timestamp operator+(const double seconds) const {
        const auto delta = static_cast<int64_t>(seconds * S_MICRO_SECONDS_PER_SECOND);
        return Timestamp(m_microSecondsSinceEpoch + delta);
    }

    /**
     * @brief 重载<符号
     */
    bool operator<(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch < other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载<=符号
     */
    bool operator<=(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch <= other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载>符号
     */
    bool operator>(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch > other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载>=符号
     */
    bool operator>=(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch >= other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载==符号
     */
    bool operator==(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch == other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载!=符号
     */
    bool operator!=(const Timestamp& other) const {
        return this->m_microSecondsSinceEpoch != other.m_microSecondsSinceEpoch;
    }

    /**
     * @brief 重载输出操作符，打印格式化时间
     * @param os 输出流
     * @param timestamp 需要输出的时间戳
     * @return 输出流
     *
     * @note 将输出toString()函数的格式
     */
    friend std::ostream& operator<<(std::ostream& os, const Timestamp& timestamp) {
        os << timestamp.toString();
        return os;
    }

private:
    int64_t m_microSecondsSinceEpoch; ///< 微秒时间戳
};

/**
 * @class Date
 * @brief 日期类（仅保留年月日，不含具体时间）
 *
 * 可结合 Timestamp 使用，用于比较、显示日期等。
 */
class Date : public Copyable {
public:
    /**
     * @brief 使用微秒时间戳构造
     * @param[in] microSecondsSinceEpoch 微秒时间戳
     */
    explicit Date(const int64_t microSecondsSinceEpoch)
        : m_microSecondsSinceEpoch(microSecondsSinceEpoch) {}

    /**
     * @brief 使用 Timestamp 构造
     * @param[in] timestamp 时间戳对象
     */
    explicit Date(const Timestamp& timestamp)
        : m_microSecondsSinceEpoch(timestamp.getMicroSecondsSinceEpoch()) {}

    /**
     * @brief 获取当前日期
     * @return 当前日期（包含年月日）
     */
    static Date Now();

    /**
     * @brief 获取微秒时间戳
     * @return 自 Unix 纪元以来的微秒数
     */
    int64_t getMicroSecondsSinceEpoch() const {
        return m_microSecondsSinceEpoch;
    }

    /**
     * @brief 转换为字符串，格式 yyyy-MM-dd
     * @return 日期字符串
     */
    std::string toString() const;

    /**
     * @brief 重载<符号
     */
    bool operator<(const Date& other) const {
        return toDateEpoch() < other.toDateEpoch();
    }

    /**
     * @brief 重载<=符号
     */
    bool operator<=(const Date& other) const {
        return toDateEpoch() <= other.toDateEpoch();
    }

    /**
     * @brief 重载>符号
     */
    bool operator>(const Date& other) const {
        return toDateEpoch() > other.toDateEpoch();
    }

    /**
     * @brief 重载>=符号
     */
    bool operator>=(const Date& other) const {
        return toDateEpoch() >= other.toDateEpoch();
    }

    /**
     * @brief 重载==符号
     */
    bool operator==(const Date& other) const {
        return toDateEpoch() == other.toDateEpoch();
    }

    /**
     * @brief 重载!=符号
     */
    bool operator!=(const Date& other) const {
        return toDateEpoch() != other.toDateEpoch();
    }

    /**
     * @brief 重载输出操作符，打印格式化日期
     * @param os 输出流
     * @param date 需要输出的日期
     * @return 输出流
     *
     * @note 将输出toString()函数的格式
     */
    friend std::ostream& operator<<(std::ostream& os, const Date& date) {
        os << date.toString();
        return os;
    }

private:
    /**
     * @brief 获取去除时间部分后的日期时间戳（秒）
     * @return 当天 00:00:00 的时间戳
     */
    time_t toDateEpoch() const;

private:
    int64_t m_microSecondsSinceEpoch; ///< 微秒时间戳
};
} // namespace zmuduo

#endif