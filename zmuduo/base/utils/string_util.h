// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_STRING_UTIL_H
#define ZMUDUO_BASE_UTILS_STRING_UTIL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include <string>
#include <vector>

/**
 * @namespace string_util
 * @brief 字符串工具类
 */
namespace zmuduo::utils::string_util {

/**
 * @brief 对 URL 的路径、查询字符串和片段进行编码
 *
 * @param[in] url 输入字符串，可以是完整 URL（含协议、主机）或仅包含 path/query/fragment
 * @param[in] isFullUrl 是否输入为完整 URL（例如 https://example.com/path?q=1#frag）,默认为true
 * @param[in] spaceAsPlus 是否将空格编码为 '+'（true 表单兼容，false 用 "%20"），默认为false
 * @return 编码后的 URL 字符串
 *
 * @note 如果 `isFullUrl` 为 true，将保留 scheme 和 host 部分不编码；
 * @note path 中的 '/' 不会被编码；
 * @note query 和 fragment 中的 '/' 会被编码；
 * @note 空格根据 `spaceAsPlus` 设置为 '+' 或 '%20'；
 *
 * @example
 *
 * @code
 * urlencode("/search?q=a b", false, true); // 结果: "/search?q=a+b"
 * urlencode("https://test.com/a b?q=1/2#frag ment");
 * // 结果: "https://test.com/a%20b?q=1%2F2#frag%20ment"
 * @endcode
 */
std::string UrlEncode(const std::string& url, bool isFullUrl = true, bool spaceAsPlus = false);
/**
 * @brief URL解码函数
 *
 * 将百分号编码的URL字符串解码为普通字符串（遵循RFC 3986标准）
 * 可选是否将"+"转换为空格（常用于表单数据处理）
 *
 * @param str 需要解码的URL编码字符串
 * @param spaceAsPlus 是否将"+"当作空格处理（默认true）
 * @return std::string 解码后的字符串
 *
 * @note 无效的百分号编码（如%x或单独的%）将保持原样输出
 * @example
 * @code
 * auto result1 = UrlDecode("Hello%20World%21")
 * // 返回 "Hello World!"<br/>
 * auto result2 = UrlDecode("Name+Value", true)
 * // 返回 "Name Value"
 * @endcode
 */
std::string UrlDecode(const std::string& str, bool spaceAsPlus = true);
/**
 * @brief 去除字符串首尾的指定字符
 *
 * 从字符串开头和末尾删除所有出现在分隔符集合中的字符，直到遇到不在分隔符集合中的字符为止
 *
 * @param str 要处理的原始字符串
 * @param delimit 包含所有分隔符的字符串（默认值：" \\t\\r\\n" 表示空格、制表符、回车、换行）
 * @return std::string 处理后的字符串
 *
 * @note 默认删除空白字符（空格、制表符、回车符、换行符）
 * @note 不会修改原始字符串
 * @note 如果字符串全部由分隔符组成，将返回空字符串
 *
 * @example
 * @code
 *   auto result1 = Trim("  hello world  ")
 *   // 返回 "hello world"
 *   auto result2 = Trim("++test++", "+")
 *   // 返回 "test"
 * @endcode
 */
std::string Trim(const std::string& str, const std::string& delimit = " \t\r\n");
/**
 * @brief 去除字符串开头的指定字符
 *
 * 从字符串开头删除所有出现在分隔符集合中的字符，直到遇到不在分隔符集合中的字符为止
 *
 * @param str 要处理的原始字符串
 * @param delimit 包含所有分隔符的字符串（默认值：" \\t\\r\\n" 表示空格、制表符、回车、换行）
 * @return std::string 处理后的字符串
 *
 * @note 默认删除空白字符（空格、制表符、回车符、换行符）
 * @note 不会修改原始字符串
 * @note 如果字符串全部由分隔符组成，将返回空字符串
 *
 * @example
 * @code
 *   auto result1 = TrimLeft("  hello world  ")
 *   // 返回 "hello world  "
 *   auto result2 = TrimLeft("++test++", "+")
 *   // 返回 "test++"
 * @endcode
 */
std::string TrimLeft(const std::string& str, const std::string& delimit = " \t\r\n");
/**
 * @brief 去除字符串末尾的指定字符
 *
 * 从字符串末尾删除所有出现在分隔符集合中的字符，直到遇到不在分隔符集合中的字符为止
 *
 * @param str 要处理的原始字符串
 * @param delimit 包含所有分隔符的字符串（默认值：" \\t\\r\\n" 表示空格、制表符、回车、换行）
 * @return std::string 处理后的字符串
 *
 * @note 默认删除空白字符（空格、制表符、回车符、换行符）
 * @note 不会修改原始字符串
 * @note 如果字符串全部由分隔符组成，将返回空字符串
 *
 * @example
 * @code
 * auto result1 = TrimRight("  hello world  ")
 * // 返回 "  hello world"
 * auto result2 = TrimRight("++test++", "+")
 * // 返回 "++test"
 * @endcode
 */
std::string TrimRight(const std::string& str, const std::string& delimit = " \t\r\n");
/**
 * @brief 将字符串按指定分隔符拆分为子字符串列表
 *
 * 此函数将一个字符串按照给定的分隔符拆分成多个子字符串，并返回包含这些子字符串的向量。
 * 连续的分隔符会产生空字符串元素，例如拆分 "a,,b" 会得到 ["a", "", "b"]。
 *
 * @param[in] s 要拆分的输入字符串
 * @param[in] delimiter 用作分隔符的字符
 * @return std::vector<std::string> 包含拆分结果的字符串向量
 *
 * @note 空字符串输入将返回包含一个空字符串的向量（[""]）
 * @note 分隔符可以是任意字符，包括空格、逗号等
 * @note 性能考虑：此实现使用 std::getline 进行拆分，适用于大多数常规场景
 *
 * @example
 * @code
 * // 示例用法
 * auto result1 = Split("apple,banana,orange", ',');
 * // 返回 {"apple", "banana", "orange"}
 * auto result2 = Split("one|two||four", '|');
 * // 返回 {"one", "two", "", "four"}
 * auto result3 = Split("", ',');
 * // 返回 {""}
 * @endcode
 */
std::vector<std::string> Split(const std::string& s, char delimiter);

/**
 * @brief 将字符串按指定分隔符拆分为子字符串列表
 *
 * 此函数将一个字符串按照给定的分隔符拆分成多个子字符串，并返回包含这些子字符串的向量。
 * 连续的分隔符会产生空字符串元素，例如拆分 "a,,b" 会得到 ["a", "", "b"]。
 *
 * @param[in] str 要拆分的输入字符串
 * @param[in] delimiter 用作分隔符的字符串
 * @return std::vector<std::string> 包含拆分结果的字符串向量
 *
 * @note 空字符串输入将返回包含一个空字符串的向量（[""]）
 * @note 分隔符可以是任意字符，包括空格、逗号等
 * @note 性能考虑：此实现使用 std::getline 进行拆分，适用于大多数常规场景
 *
 * @example
 * @code
 * // 示例用法
 * auto result1 = Split("apple,banana,orange", ",");
 * // 返回 {"apple", "banana", "orange"}
 * auto result2 = Split("one|two||four", "|");
 * // 返回 {"one", "two", "", "four"}
 * auto result3 = Split("aea", "e");
 * // 返回 {"a","a"}
 * @endcode
 */
std::vector<std::string> Split(const std::string& str, const std::string& delimiter);

/**
 * @brief 检查字符串是否以指定前缀开头
 *
 * 此函数检查一个字符串是否以给定的前缀开头，比较时区分大小写。
 * 如果前缀为空字符串，则任何输入字符串都将被视为匹配（返回true）。
 *
 * @param[in] str 要检查的输入字符串
 * @param[in] prefix 要匹配的前缀字符串
 * @return bool 如果字符串以指定前缀开头则返回true，否则返回false
 *
 * @note 此函数执行区分大小写的比较
 * @note 如果prefix为空字符串，函数总是返回true
 * @note 如果str比prefix短，函数直接返回false
 * @note 性能考虑：此实现使用std::string::compare，具有O(n)时间复杂度（n为prefix长度）
 * @note 对于不区分大小写的版本，可考虑实现StartsWithIgnoreCase
 *
 * @example
 * @code
 * // 示例用法
 * bool result1 = StartsWith("Hello World", "Hello"); // 返回 true
 * bool result2 = StartsWith("Hello World", "hello"); // 返回 false（大小写敏感）
 * bool result3 = StartsWith("Hello World", "");      // 返回 true
 * bool result4 = StartsWith("Hello", "Hello World"); // 返回 false
 * bool result5 = StartsWith("", "");                // 返回 true
 * @endcode
 */
bool StartsWith(const std::string& str, const std::string& prefix);

}  // namespace zmuduo::utils::string_util

#endif