#ifndef ZMUDUO_INSPECT_PERFORMANCE_INSPECTOR_H
#define ZMUDUO_INSPECT_PERFORMANCE_INSPECTOR_H
#include <zmuduo/net/http/http_dispatcher.h>
#include <zmuduo/net/http/http_servlet.h>

using namespace zmuduo::net::http;

namespace zmuduo::inspect {
class PerformanceInspector : public Servlet {
  public:
    PerformanceInspector();

    void handle(const HttpRequest& request, HttpResponse& response) override;

  private:
    static void heap(const HttpRequest& request, HttpResponse& response);
    static void growth(const HttpRequest& request, HttpResponse& response);
    static void profile(const HttpRequest& request, HttpResponse& response);
    static void memstats(const HttpRequest& request, HttpResponse& response);
    static void memhistogram(const HttpRequest& request, HttpResponse& response);
    static void releaseFreeMemory(const HttpRequest& request, HttpResponse& response);

  private:
    ServletDispatcher m_dispatcher;
};
}  // namespace zmuduo::inspect

#endif