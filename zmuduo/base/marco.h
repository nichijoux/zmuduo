#ifndef ZMUDUO_BASE_MARCO_H
#define ZMUDUO_BASE_MARCO_H

#if defined __GNUC__ || defined __llvm__
/// @brief LIKCLY 宏的封装, 告诉编译器优化,条件大概率成立
#    define ZMUDUO_LIKELY(x) __builtin_expect(!!(x), 1)
/// @brief LIKCLY 宏的封装, 告诉编译器优化,条件大概率不成立
#    define ZMUDUO_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#    define ZMUDUO_LIKELY(x) (x)
#    define ZMUDUO_UNLIKELY(x) (x)
#endif

#endif