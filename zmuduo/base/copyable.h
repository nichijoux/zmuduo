// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_COPYABLE_H
#define ZMUDUO_BASE_COPYABLE_H

namespace zmuduo {
/**
 * @brief 可拷贝类
 */
class Copyable {
  protected:
    Copyable()  = default;
    ~Copyable() = default;

    Copyable(const Copyable&)            = default;
    Copyable& operator=(const Copyable&) = default;
};
}  // namespace zmuduo

#endif