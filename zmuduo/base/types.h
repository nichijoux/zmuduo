// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_TYPES_H
#define ZMUDUO_BASE_TYPES_H

#include <charconv>
#include <sstream>

namespace zmuduo {
// Taken from google-protobuf stubs/common.h
//
// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// http://code.google.com/p/protobuf/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda) and others
//
// Contains basic types and utilities used by the rest of the library.

//
// Use implicit_cast as a safe version of static_cast or const_cast
// for upcasting in the type hierarchy (i.e. casting a pointer to Foo
// to a pointer to SuperclassOfFoo or casting a pointer to Foo to
// a const pointer to Foo).
// When you use implicit_cast, the compiler checks that the cast is safe.
// Such explicit implicit_casts are necessary in surprisingly many
// situations where C++ demands an exact type match instead of an
// argument type convertable to a target type.
//
// The From type can be inferred, so the preferred syntax for using
// implicit_cast is the same as for static_cast etc.:
//
//   implicit_cast<ToType>(expr)
//
// implicit_cast would have been part of the C++ standard library,
// but the proposal was submitted too late.  It will probably make
// its way into the language in the future.
template <typename To, typename From>
inline To implicit_cast(From const& f) {
    return f;
}

// When you upcast (that is, cast a pointer from type Foo to type
// SuperclassOfFoo), it's fine to use implicit_cast<>, since upcasts
// always succeed.  When you downcast (that is, cast a pointer from
// type Foo to type SubclassOfFoo), static_cast<> isn't safe, because
// how do you know the pointer is really of type SubclassOfFoo?  It
// could be a bare Foo, or of type DifferentSubclassOfFoo.  Thus,
// when you downcast, you should use this macro.  In debug mode, we
// use dynamic_cast<> to double-check the downcast is legal (we die
// if it's not).  In normal mode, we do the efficient static_cast<>
// instead.  Thus, it's important to test in debug mode to make sure
// the cast is legal!
//    This is the only place in the code we should use dynamic_cast<>.
// In particular, you SHOULDN'T be using dynamic_cast<> in order to
// do RTTI (eg code like this:
//    if (dynamic_cast<Subclass1>(foo)) HandleASubclass1Object(foo);
//    if (dynamic_cast<Subclass2>(foo)) HandleASubclass2Object(foo);
// You should design the code some other way not to need this.

template <typename To, typename From>  // use like this: down_cast<T*>(foo);
inline To down_cast(From* f)           // so we only accept pointers
{
    // Ensures that To is a sub-type of From *.  This test is here only
    // for compile-time type checking, and has no overhead in an
    // optimized build at run-time, as it will be optimized away
    // completely.
    if (false) {
        implicit_cast<From*, To>(0);
    }

#if !defined(NDEBUG) && !defined(GOOGLE_PROTOBUF_NO_RTTI)
    assert(f == nullptr || dynamic_cast<To>(f) != nullptr);  // RTTI: debug mode only!
#endif
    return static_cast<To>(f);
}

/**
 * @brief 解析字符串为指定类型，支持可选回退值。
 *
 * 根据目标类型 `T` 的特性选择高效解析方式：<br/>
 * 整型使用 `std::from_chars`，高效且无异常。<br/>
 * 浮点型使用 `std::strtof`、`strtod` 或 `strtold`，检查 `errno` 和范围。<br/>
 * 其他类型通过 `std::istringstream` 提取，支持流式输入。
 *
 * @tparam T 目标类型。
 * @tparam has_fallback 是否提供回退值（默认 `false`）。
 * @tparam Fallback 回退值类型（默认 `T`）。
 * @param[in] str 输入字符串。
 * @param[in] fallback 回退值（默认构造 `Fallback{}`），仅当 `has_fallback` 为 `true` 时生效。
 * @return T 解析结果，失败时返回默认构造的 `T{}` 或 `static_cast<T>(fallback)`。
 *
 * @note 整型解析失败返回 `T{}` 或 `fallback`，浮点型检查 `errno` 和转换范围。
 * @note 其他类型依赖 `operator>>`，需确保 `T` 支持流提取。
 * @note 空字符串或无效输入可能导致默认值或回退值。
 *
 * @example
 *
 * @code
 * int i = parse_value<int>("123"); // 返回 123
 * int j = parse_value<int, true>("abc", 42); // 返回 42（回退值）
 * double d = parse_value<double>("3.14"); // 返回 3.14
 * std::string s = parse_value<std::string>("hello"); // 返回 "hello"
 * @endcode
 */
template <typename T, bool has_fallback = false, typename Fallback = T>
T parse_value(const std::string& str, Fallback&& fallback = Fallback{}) {
    if constexpr (std::is_integral_v<T>) {
        // 数值类型
        T value{};
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
        if (ec == std::errc()) {
            return value;
        } else if constexpr (has_fallback) {
            return static_cast<T>(std::forward<Fallback>(fallback));
        } else {
            return T{};
        }
    } else if constexpr (std::is_floating_point_v<T>) {
        // 浮点数
        const char* begin = str.c_str();
        char*       end   = nullptr;
        errno             = 0;

        T result{};
        if constexpr (std::is_same_v<T, float>) {
            result = std::strtof(begin, &end);
        } else if constexpr (std::is_same_v<T, double>) {
            result = std::strtod(begin, &end);
        } else if constexpr (std::is_same_v<T, long double>) {
            result = std::strtold(begin, &end);
        } else {
            if constexpr (has_fallback)
                return static_cast<T>(std::forward<Fallback>(fallback));
            else
                return T{};
        }
        // 转换失败
        if (begin == end || errno != 0) {
            if constexpr (has_fallback) {
                return static_cast<T>(std::forward<Fallback>(fallback));
            } else {
                return T{};
            }
        }
        return result;
    } else {
        // 其他类型
        std::istringstream iss(str);
        T                  value{};
        iss >> value;
        // 转换失败
        if (iss.fail()) {
            if constexpr (has_fallback) {
                return static_cast<T>(std::forward<Fallback>(fallback));
            } else {
                return T{};
            }
        }
        return value;
    }
}

/**
 * @brief 将源类型转换为目标类型，无回退值。
 *
 * 使用 `std::stringstream` 将源类型 `S` 序列化为字符串，再通过 `parse_value` 解析为目标类型 `T`。
 * 转换失败时返回默认构造的 `T{}`。
 *
 * @tparam T 目标类型。
 * @tparam S 源类型。
 * @param[in] arg 源值。
 * @return T 转换结果，失败时返回 `T{}`。
 *
 * @note 源类型需支持 `operator<<` 序列化，目标类型需支持 `parse_value` 解析。
 * @note 使用 `std::stringstream` 可能引入性能开销，适合非性能敏感场景。
 * @note 函数标记为 `noexcept`，保证无异常。
 *
 * @example
 *
 * @code
 * int i = lexical_cast<int>("123"); // 返回 123
 * double d = lexical_cast<double>(42); // 返回 42.0
 * std::string s = lexical_cast<std::string>(123); // 返回 "123"
 * @endcode
 */
template <typename T, typename S>
T lexical_cast(const S& arg) noexcept {
    std::stringstream ss;
    ss << arg;
    return parse_value<T, false>(ss.str());
}

/**
 * @brief 将源类型转换为目标类型，支持回退值。
 *
 * 使用 `std::stringstream` 将源类型 `S` 序列化为字符串，再通过 `parse_value` 解析为目标类型 `T`。
 * 转换失败时返回 `static_cast<T>(fallback)`。
 *
 * @tparam T 目标类型。
 * @tparam S 源类型。
 * @tparam Fallback 回退值类型。
 * @param[in] arg 源值。
 * @param[in] fallback 回退值。
 * @return T 转换结果，失败时返回 `static_cast<T>(fallback)`。
 *
 * @note 源类型需支持 `operator<<` 序列化，目标类型需支持 `parse_value` 解析。
 * @note 使用 `std::stringstream` 可能引入性能开销，适合非性能敏感场景。
 *
 * @example
 *
 * @code
 * int i = lexical_cast&lt;int&gt;("abc", 42); // 返回 42（回退值）
 * double d = lexical_cast&lt;double&gt;("3.14", 0.0); // 返回 3.14
 * std::string s = lexical_cast&lt;std::string&gt;(123, "default"); // 返回 "123"
 * @endcode
 */
template <typename T, typename S, typename Fallback>
T lexical_cast(const S& arg, Fallback&& fallback) {
    std::stringstream ss;
    ss << arg;
    return parse_value<T, true>(ss.str(), static_cast<T>(std::forward<Fallback>(fallback)));
}

// 特化版本：字符串到字符串的直接转换
template <>
inline std::string lexical_cast<std::string, std::string>(const std::string& arg) noexcept {
    return arg;
}

// 特化版本：C风格字符串到字符串的直接转换
template <>
inline std::string lexical_cast<std::string, const char*>(const char* const& arg) noexcept {
    return arg;
}

// 特化版本：字符串到C风格字符串（不支持，因为不安全）
template <>
const char* lexical_cast<const char*, std::string>(const std::string& arg) noexcept = delete;
}  // namespace zmuduo

#endif