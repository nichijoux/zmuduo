// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_HASH_UTIL_H
#define ZMUDUO_BASE_UTILS_HASH_UTIL_H

#include <string>

/**
 * @namespace hash_util
 * @brief 哈希与编码工具
 *
 * 提供 Base64 编码/解码、SHA1 哈希计算、十六进制转二进制、随机字符串生成等常用工具函数。<br/>
 */
namespace zmuduo::utils::hash_util {
/**
 * @brief 解码 Base64 编码的字符串。
 *
 * @param[in] src Base64 编码的输入字符串。
 * @return 解码后的原始字符串（二进制数据以 std::string 表示）。
 */
std::string Base64decode(std::string_view src);

/**
 * @brief 对原始数据进行 Base64 编码。
 *
 * @param[in] data 指向原始数据的指针。
 * @param[in] length 数据长度（字节数）。
 * @return 编码后的 Base64 字符串。
 */
std::string Base64encode(const void* data, size_t length);

/**
 * @brief 对字符串数据进行 Base64 编码
 *
 * @param[in] data 输入字符串（支持 std::string 和 string_view）。
 * @return 编码后的 Base64 字符串。
 */
inline std::string Base64encode(std::string_view data) {
    return Base64encode(data.data(), data.size());
}

/**
 * @brief 将十六进制字符串转换为对应的二进制数据。
 *
 * @param[in] hex 仅包含十六进制字符（0-9, a-f, A-F）的字符串。
 * @return 转换后的二进制数据，以 std::string 形式表示。
 * @throws std::invalid_argument 如果输入包含非法字符或长度不为偶数。
 */
std::string HexToBinary(std::string_view hex);

/**
 * @brief 计算原始数据的 SHA1 哈希值。
 *
 * @param[in] data 指向数据的指针。
 * @param[in] length 数据长度（字节数）。
 * @return SHA1 哈希结果，作为十六进制字符串返回（长度为 40 字符）。
 */
std::string SHA1sum(const void* data, size_t length);

/**
 * @brief 计算字符串的 SHA1 哈希值。
 *
 * @param[in] string 输入字符串。
 * @return SHA1 哈希结果，作为十六进制字符串返回（长度为 40 字符）。
 */
inline std::string SHA1sum(const std::string& string) {
    return SHA1sum(string.c_str(), string.size());
}

/**
 * @brief 计算 MD5 哈希
 * @param input 输入字符串
 * @param bitLength 结果位数，支持 16 或 32（默认 32）
 * @param toUpper 是否输出为大写（默认 false）
 * @return MD5 哈希字符串
 */
std::string MD5(const std::string& input, int bitLength = 32, bool toUpper = false);

/**
 * @brief 生成指定长度的随机字符串。
 *
 * @param[in] length 随机字符串的长度。
 * @return int8_t范围内数据组成的随机字符串。
 */
std::string RandomString(size_t length);
} // namespace zmuduo::utils::hash_util

#endif