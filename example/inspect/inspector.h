#ifndef ZMUDUO_INSPECT_INSPECTOR_H
#define ZMUDUO_INSPECT_INSPECTOR_H

#include <string>
#include <vector>
#include <zmuduo/net/address.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/http/http_server.h>

namespace zmuduo::inspect {

using namespace zmuduo::net;
using namespace zmuduo::net::http;

class Inspector {
  public:
    Inspector(EventLoop* loop, const Address::Ptr& httpAddr, const std::string& name);

    ~Inspector() = default;

    void start() {
        m_server.start();
    }

  private:
    static void help(const HttpRequest& req, HttpResponse& res);

  private:
    HttpServer m_server;
};
}  // namespace zmuduo::inspect

#endif