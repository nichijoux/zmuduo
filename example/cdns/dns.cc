#include "resolver.h"
#include <cstdio>
#include <zmuduo/base/logger.h>
#include <zmuduo/net/event_loop.h>

// using namespace std;
using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::cdns;

EventLoop* g_loop;
int        count = 0;
int        total = 0;

void quit() {
    g_loop->quit();
}

void resolveCallback(const std::string& host, const Address::Ptr& addr) {
    ZMUDUO_LOG_FMT_IMPORTANT("resolveCallback %s -> %s", host.c_str(), addr->toString().c_str());
    if (++count == total) {
        quit();
    }
}

void resolve(Resolver* res, const std::string& host) {
    res->resolve(host,
                 [host](auto&& PH1) { resolveCallback(host, std::forward<decltype(PH1)>(PH1)); });
}

int main(int argc, char* argv[]) {
    EventLoop loop;
    loop.runAfter(10, quit);
    g_loop = &loop;
    Resolver resolver(&loop, argc == 1 ? Resolver::kDNSonly : Resolver::kDNSandHostsFile);
    if (argc == 1) {
        total = 3;
        resolve(&resolver, "www.chenshuo.com");
        resolve(&resolver, "www.example.com");
        resolve(&resolver, "www.google.com");
    } else {
        total = argc - 1;
        for (int i = 1; i < argc; ++i) {
            resolve(&resolver, argv[i]);
        }
    }
    loop.loop();
}
