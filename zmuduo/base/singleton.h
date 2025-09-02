// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_BASE_SINGLETON_H
#define ZMUDUO_BASE_SINGLETON_H

namespace zmuduo {
template <class T>
class Singleton final {
public:
    static T& GetInstance() {
        static T v;
        return v;
    }

    Singleton(const Singleton&)            = delete;
    Singleton& operator=(const Singleton&) = delete;

protected:
    Singleton()  = default;
    ~Singleton() = default;
};
} // namespace zmuduo

#endif