#ifndef ZMUDUO_RESOLVER_H
#define ZMUDUO_RESOLVER_H

#include <functional>
#include <utility>
#include <zmuduo/base/nocopyable.h>
#include <zmuduo/net/address.h>
#include <zmuduo/net/event_loop.h>

extern "C" {
struct hostent;
struct ares_channeldata;
typedef struct ares_channeldata* ares_channel;
}

namespace zmuduo::net::cdns {
class Resolver : NoCopyable {
  public:
    using Callback = std::function<void(const Address::Ptr&)>;

    enum Option {
        kDNSandHostsFile,
        kDNSonly,
    };

    explicit Resolver(EventLoop* loop, Option opt = kDNSandHostsFile);

    ~Resolver();

    bool resolve(const std::string& hostname, const Callback& cb);

  private:
    struct QueryData {
        Resolver* owner;
        Callback  callback;
        QueryData(Resolver* o, Callback cb) : owner(o), callback(std::move(cb)) {}
    };

    EventLoop*                                      loop_;
    ares_channel                                    ctx_;
    bool                                            timerActive_;
    typedef std::map<int, std::unique_ptr<Channel>> ChannelList;
    ChannelList                                     channels_;

    void onRead(int sockfd, const Timestamp& t);
    void onTimer();
    void onQueryResult(int status, struct hostent* result, const Callback& cb);
    void onSockCreate(int sockfd, int type);
    void onSockStateChange(int sockfd, bool read, bool write);

    static void ares_host_callback(void* data, int status, int timeouts, struct hostent* hostent);
    static int  ares_sock_create_callback(int sockfd, int type, void* data);
    static void ares_sock_state_callback(void* data, int sockfd, int read, int write);
};
}  // namespace zmuduo::net::cdns

#endif  // ZMUDUO_RESOLVER_H
