// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_BASE_UTILS_COMMON_UTIL_H
#define ZMUDUO_BASE_UTILS_COMMON_UTIL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include <memory>
#include <stdexcept>
#include <string>

namespace zmuduo::utils {
/**
 * @brief 通用工具类
 */
class CommonUtil : NoCopyable, NoMoveable {
  public:
    /**
     * @brief 检查原始指针是否为 nullptr，若非空则返回其引用。
     *
     * @tparam T 指针所指向的类型。
     * @param[in] ptr 原始指针（T*）。
     * @param[in] message 指针为空时抛出异常的提示信息。
     * @return const T& 非空指针所指对象的引用。
     *
     * @throws std::invalid_argument 当 ptr 为 nullptr 时抛出异常。
     */
    template <typename T>
    inline static const T& CheckNotNull(T*                 ptr,
                                        const std::string& message = "checkNotNull: nullptr") {
        if (!ptr) {
            throw std::invalid_argument(message);
        }
        return *ptr;
    }

    /**
     * @brief 检查 shared_ptr 是否为空，若非空则返回其引用。
     *
     * @tparam T 智能指针所管理的对象类型。
     * @param[in] ptr std::shared_ptr<T> 智能指针。
     * @param[in] message 指针为空时抛出异常的提示信息。
     * @return const T& 非空智能指针所指对象的引用。
     *
     * @throws std::invalid_argument 当 ptr 为 nullptr 时抛出异常。
     */
    template <typename T>
    inline static const T& CheckNotNull(const std::shared_ptr<T>& ptr,
                                        const std::string& message = "checkNotNull: nullptr") {
        if (!ptr) {
            throw std::invalid_argument(message);
        }
        return *ptr;
    }

    /**
     * @brief 检查 unique_ptr 是否为空，若非空则返回其引用。
     *
     * @tparam T 智能指针所管理的对象类型。
     * @param[in] ptr std::unique_ptr<T> 智能指针。
     * @param[in] message 指针为空时抛出异常的提示信息。
     * @return const T& 非空智能指针所指对象的引用。
     *
     * @throws std::invalid_argument 当 ptr 为 nullptr 时抛出异常。
     */
    template <typename T>
    inline static const T& CheckNotNull(const std::unique_ptr<T>& ptr,
                                        const std::string& message = "checkNotNull: nullptr") {
        if (!ptr) {
            throw std::invalid_argument(message);
        }
        return *ptr;
    }
};

}  // namespace zmuduo::utils

#endif