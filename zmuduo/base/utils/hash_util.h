// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_HASH_UTIL_H
#define ZMUDUO_BASE_UTILS_HASH_UTIL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include <string>

namespace zmuduo::utils {
/**
 * @class HashUtil
 * @brief 哈希与编码工具类
 *
 * 提供 Base64 编码/解码、SHA1 哈希计算、十六进制转二进制、随机字符串生成等常用工具函数。<br/>
 * 所有方法均为静态方法；此类不可拷贝、不可移动。
 */
class HashUtil : NoCopyable, NoMoveable {
  public:
    /**
     * @brief 解码 Base64 编码的字符串。
     *
     * @param[in] src Base64 编码的输入字符串。
     * @return 解码后的原始字符串（二进制数据以 std::string 表示）。
     */
    static std::string Base64decode(std::string_view src);

    /**
     * @brief 对原始数据进行 Base64 编码。
     *
     * @param[in] data 指向原始数据的指针。
     * @param[in] length 数据长度（字节数）。
     * @return 编码后的 Base64 字符串。
     */
    static std::string Base64encode(const void* data, size_t length);

    /**
     * @brief 对字符串数据进行 Base64 编码
     *
     * @param[in] data 输入字符串（支持 std::string 和 string_view）。
     * @return 编码后的 Base64 字符串。
     */
    static std::string Base64encode(std::string_view data) {
        return Base64encode(data.data(), data.size());
    }

    /**
     * @brief 将十六进制字符串转换为对应的二进制数据。
     *
     * @param[in] hex 仅包含十六进制字符（0-9, a-f, A-F）的字符串。
     * @return 转换后的二进制数据，以 std::string 形式表示。
     * @throws std::invalid_argument 如果输入包含非法字符或长度不为偶数。
     */
    static std::string HexToBinary(std::string_view hex);

    /**
     * @brief 计算字符串的 SHA1 哈希值。
     *
     * @param[in] string 输入字符串。
     * @return SHA1 哈希结果，作为十六进制字符串返回（长度为 40 字符）。
     */
    static std::string SHA1sum(const std::string& string) {
        return SHA1sum(string.c_str(), string.size());
    }

    /**
     * @brief 计算原始数据的 SHA1 哈希值。
     *
     * @param[in] data 指向数据的指针。
     * @param[in] length 数据长度（字节数）。
     * @return SHA1 哈希结果，作为十六进制字符串返回（长度为 40 字符）。
     */
    static std::string SHA1sum(const void* data, size_t length);

    /**
     * @brief 计算 MD5 哈希
     * @param input 输入字符串
     * @param bitLength 结果位数，支持 16 或 32（默认 32）
     * @param toUpper 是否输出为大写（默认 false）
     * @return MD5 哈希字符串
     */
    static std::string MD5(const std::string& input, int bitLength = 32, bool toUpper = false);

    /**
     * @brief 生成指定长度的随机字符串。
     *
     * @param[in] length 随机字符串的长度。
     * @return int8_t范围内数据组成的随机字符串。
     */
    static std::string RandomString(size_t length);

  private:
    /**
     * @brief 执行 SHA1 的循环左移操作（仅供 SHA1 算法内部使用）。
     *
     * @param[in] bits 左移位数。
     * @param[in] word 原始 32 位整数。
     * @return 左移后的结果。
     */
    static inline uint32_t SHA1CircularShift(uint32_t bits, uint32_t word) {
        return ((word << bits) | (word >> (32 - bits)));
    }

    /**
     * @brief 执行一次 SHA1 的核心变换（对 512bit 块进行处理）。
     *
     * @param[in,out] state SHA1 状态数组（5 个 32 位整型）。
     * @param[in] buffer 64 字节输入块。
     */
    static void SHA1Transform(uint32_t state[5], const uint8_t buffer[64]);

    /**
     * @brief 将 SHA1 的状态数组转换为最终的哈希字符串。
     *
     * @param[in] state SHA1 的 5 个 32 位状态值。
     * @return SHA1 哈希值的十六进制字符串表示。
     */
    static std::string SHA1Final(const uint32_t state[5]);

    /**
     * @brief MD5 核心压缩函数，对每个 64 字节块进行处理。
     *
     * @param block 64 字节输入块（512 位），已按 MD5 填充规则填充。
     * @param A 初始 A 寄存器值，处理后累加更新。
     * @param B 初始 B 寄存器值，处理后累加更新。
     * @param C 初始 C 寄存器值，处理后累加更新。
     * @param D 初始 D 寄存器值，处理后累加更新。
     *
     * @note 这是 MD5 的主处理函数，包含四轮迭代（每轮 16 次），共 64 次操作，
     *       每轮使用不同的逻辑函数（F/G/H/I）和常量。
     */
    static void
    md5Process(const uint8_t block[64], uint32_t& A, uint32_t& B, uint32_t& C, uint32_t& D);
};
}  // namespace zmuduo::utils

#endif