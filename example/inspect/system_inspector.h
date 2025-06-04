#ifndef ZMUDUO_INSPECT_SYSTEM_INSPECTOR_H
#define ZMUDUO_INSPECT_SYSTEM_INSPECTOR_H
#include <zmuduo/net/http/http_dispatcher.h>
#include <zmuduo/net/http/http_servlet.h>

using namespace zmuduo::net::http;

namespace zmuduo::inspect {
class SystemInspector : public Servlet {
  public:
    SystemInspector();

    void handle(const HttpRequest& request, HttpResponse& response) override;

  private:
    static void loadavg(const HttpRequest& request, HttpResponse& response);
    static void version(const HttpRequest& request, HttpResponse& response);
    static void cpuinfo(const HttpRequest& request, HttpResponse& response);
    static void meminfo(const HttpRequest& request, HttpResponse& response);
    static void stat(const HttpRequest& request, HttpResponse& response);

  private:
    ServletDispatcher m_dispatcher;
    static void       overview(const HttpRequest& request, HttpResponse& response);
};
}  // namespace zmuduo::inspect

#endif