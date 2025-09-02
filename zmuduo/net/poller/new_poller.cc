// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#include "zmuduo/net/poller.h"
#include "zmuduo/net/poller/epoll_poller.h"
#include "zmuduo/net/poller/poll_poller.h"
#include "zmuduo/net/poller/select_poller.h"
#include <cstdlib>

namespace zmuduo::net {
Poller* Poller::newPoller(EventLoop* loop) {
    if (::getenv("ZMUDUO_USE_POLL")) {
        return new PollPoller(loop);
    }
    if (::getenv("ZMUDUO_USE_SELECT")) {
        return new SelectPoller(loop);
    }
    return new EpollPoller(loop);
};
} // namespace zmuduo::net