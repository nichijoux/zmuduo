#include "zmuduo/net/poller.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/event_loop.h"

namespace zmuduo::net {
void Poller::assertInLoopThread() const {
    m_ownerLoop->assertInLoopThread();
}

bool Poller::hasChannel(const Channel* channel) const {
    const auto it = m_channels.find(channel->getFD());
    return it != m_channels.end() && it->second == channel;
}
}  // namespace zmuduo::net