#ifndef ZMUDUO_BASE_NO_MOVEABLE_H
#define ZMUDUO_BASE_NO_MOVEABLE_H

namespace zmuduo {
/**
 * 不可移动类,将删除移动函数
 */
class NoMoveable {
  public:
    NoMoveable()  = default;
    ~NoMoveable() = default;

    NoMoveable(NoMoveable&&)            = delete;
    NoMoveable& operator=(NoMoveable&&) = delete;
};
}  // namespace zmuduo

#endif