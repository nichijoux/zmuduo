// Original code - Copyright (c) sylar-yin —— sylar
// Modified by nichijoux

#ifndef ZMUDUO_NET_ENDIAN_H
#define ZMUDUO_NET_ENDIAN_H

#include <byteswap.h>
#include <cstdint>
#include <endian.h>
#include <type_traits>

/**
 * @def ZMUDUO_LITTLE_ENDIAN
 * @brief 表示小端字节序的常量值。
 */
#define ZMUDUO_LITTLE_ENDIAN 1

/**
 * @def ZMUDUO_BIG_ENDIAN
 * @brief 表示大端字节序的常量值。
 */
#define ZMUDUO_BIG_ENDIAN 2

namespace zmuduo::net {

/**
 * @brief 根据类型大小执行字节序转换。
 * @tparam T 输入数据类型，必须为 uint8_t, uint16_t, uint32_t 或 uint64_t。
 * @param[in] value 需要转换字节序的输入值。
 * @return 转换后的值，保持输入类型。
 * @note 该函数通过模板特化处理不同大小的整数类型，使用系统提供的 bswap_16, bswap_32 或 bswap_64
 * 进行转换。
 * @note 对于 8 位类型（uint8_t），直接返回原值，因为单字节无需转换。
 */
template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint8_t), T>::type byteswap(T value) {
    return value;
}

/**
 * @brief 16 位整数的字节序转换。
 * @tparam T 输入数据类型，必须为 uint16_t 或等效类型。
 * @param[in] value 需要转换字节序的 16 位值。
 * @return 转换后的 16 位值。
 * @note 使用 bswap_16 实现字节序翻转。
 */
template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint16_t), T>::type byteswap(T value) {
    return (T)bswap_16((uint16_t)value);
}

/**
 * @brief 32 位整数的字节序转换。
 * @tparam T 输入数据类型，必须为 uint32_t 或等效类型。
 * @param[in] value 需要转换字节序的 32 位值。
 * @return 转换后的 32 位值。
 * @note 使用 bswap_32 实现字节序翻转。
 */
template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint32_t), T>::type byteswap(T value) {
    return (T)bswap_32((uint32_t)value);
}

/**
 * @brief 64 位整数的字节序转换。
 * @tparam T 输入数据类型，必须为 uint64_t 或等效类型。
 * @param[in] value 需要转换字节序的 64 位值。
 * @return 转换后的 64 位值。
 * @note 使用 bswap_64 实现字节序翻转。
 */
template <class T>
typename std::enable_if<sizeof(T) == sizeof(uint64_t), T>::type byteswap(T value) {
    return (T)bswap_64((uint64_t)value);
}

/**
 * @def ZMUDUO_BYTE_ORDER
 * @brief 表示当前系统的字节序（ZMUDUO_LITTLE_ENDIAN 或 ZMUDUO_BIG_ENDIAN）。
 * @note 根据系统宏 BYTE_ORDER 自动定义为大端或小端。
 */
#if BYTE_ORDER == BIG_ENDIAN
#    define ZMUDUO_BYTE_ORDER ZMUDUO_BIG_ENDIAN
#else
#    define ZMUDUO_BYTE_ORDER ZMUDUO_LITTLE_ENDIAN
#endif

#if ZMUDUO_BYTE_ORDER == ZMUDUO_BIG_ENDIAN

/**
 * @brief 在大端系统上处理小端字节序数据（无需转换）。
 * @tparam T 输入数据类型，必须为 uint8_t, uint16_t, uint32_t 或 uint64_t。
 * @param[in] v 输入值，假定为小端字节序。
 * @return 原值（大端系统无需转换）。
 * @note 该函数在大端系统上直接返回输入值，因为数据已是目标字节序。
 */
template <class T>
T byteSwapOnLittleEndian(T v) {
    return v;
}

/**
 * @brief 在大端系统上处理大端字节序数据（需要转换）。
 * @tparam T 输入数据类型，必须为 uint8_t, uint16_t, uint32_t 或 uint64_t。
 * @param[in] v 输入值，假定为大端字节序。
 * @return 字节序转换后的值。
 * @note 调用 byteswap 执行字节序翻转。
 */
template <class T>
T byteSwapOnBigEndian(T v) {
    return byteswap(v);
}

#else

/**
 * @brief 在小端系统上处理小端字节序数据（需要转换）。
 * @tparam T 输入数据类型，必须为 uint8_t, uint16_t, uint32_t 或 uint64_t。
 * @param[in] v 输入值，假定为小端字节序。
 * @return 字节序转换后的值。
 * @note 调用 byteswap 执行字节序翻转。
 */
template <class T>
T byteSwapOnLittleEndian(T v) {
    return byteswap(v);
}

/**
 * @brief 在小端系统上处理大端字节序数据（无需转换）。
 * @tparam T 输入数据类型，必须为 uint8_t, uint16_t, uint32_t 或 uint64_t。
 * @param[in] v 输入值，假定为大端字节序。
 * @return 原值（小端系统无需转换）。
 * @note 该函数在小端系统上直接返回输入值，因为数据已是目标字节序。
 */
template <class T>
T byteSwapOnBigEndian(T v) {
    return v;
}

#endif

}  // namespace zmuduo::net

#endif