#include "echo_service.pb.h"
#include <atomic>
#include <zmuduo/base/logger.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/rpc/rpc_server.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::rpc;

// 服务实现
class EchoServiceImpl : public echo::EchoService {
  public:
    EchoServiceImpl() : call_count_(0) {}

    void Echo(::google::protobuf::RpcController* controller,
              const ::echo::EchoRequest*         request,
              ::echo::EchoResponse*              response,
              ::google::protobuf::Closure*       done) override {
        int count = ++call_count_;
        response->set_text("[Echo] " + request->text());
        response->set_call_count(count);

        ZMUDUO_LOG_INFO << "Echo called, count: " << count;
        if (done) { done->Run(); }
    }

    void EchoTwice(::google::protobuf::RpcController* controller,
                   const ::echo::EchoRequest*         request,
                   ::echo::EchoResponse*              response,
                   ::google::protobuf::Closure*       done) override {
        int count = ++call_count_;
        response->set_text("[EchoTwice] " + request->text() + " " + request->text());
        response->set_call_count(count);

        ZMUDUO_LOG_INFO << "EchoTwice called, count: " << count;
        if (done) done->Run();
    }

  private:
    std::atomic<int> call_count_;
};

int main(int argc, char* argv[]) {
    uint16_t  port = 8501;
    EventLoop loop;
    auto      serverAddr = IPv4Address::Create("127.0.0.1", port);

    // 创建RPC服务器
    RpcServer server(&loop, serverAddr);

    // 设置注册中心地址
    auto registryAddr = IPv4Address::Create("127.0.0.1", 8500);
    server.setRegistryAddress(registryAddr);

    // 注册服务实现
    EchoServiceImpl echoService;
    server.registerService(&echoService);

    // 启动服务器
    server.start();
    ZMUDUO_LOG_INFO << "EchoServer started on port " << port;
    loop.loop();

    return 0;
}