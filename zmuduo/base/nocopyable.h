// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_NO_COPYABLE_H
#define ZMUDUO_BASE_NO_COPYABLE_H

namespace zmuduo {
/**
 * 不可拷贝类,将删除拷贝函数
 */
class NoCopyable {
  protected:
    NoCopyable()  = default;
    ~NoCopyable() = default;

  public:
    NoCopyable(const NoCopyable&)            = delete;
    NoCopyable& operator=(const NoCopyable&) = delete;
};
}  // namespace zmuduo

#endif