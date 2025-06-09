#include "echo_service.pb.h"
#include "zmuduo/base/logger.h"

#include "zmuduo/net/rpc/rpc_channel.h"
#include "zmuduo/net/rpc/rpc_core.h"
#include <iostream>
#include <zmuduo/net/event_loop.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::rpc;

#ifdef NO_EXTEND
class EchoClient {
  public:
    EchoClient(EventLoop* loop, const Address::Ptr& registryAddr) : client_(loop, registryAddr) {}

    void callEcho(const std::string& text) {
        client_.getEventLoop()->runInLoop([this, &text]() { callEchoInLoop(text); });
    }

    void callEchoTwice(const std::string& text) {
        client_.getEventLoop()->runInLoop([this, &text]() { callEchoTwiceInLoop(text); });
    }

  private:
    void callEchoInLoop(const std::string& text) {
        client_.callService("EchoService", [text](const std::shared_ptr<RpcChannel>& channel) {
            if (!channel) {
                std::cerr << "Channel is null\n";
                return;
            }
            // 创建stub
            echo::EchoService::Stub stub(channel.get());
            // 创建resposne
            echo::EchoRequest request;
            request.set_text(text);

            RpcCaller::Call<echo::EchoResponse>(&stub, &echo::EchoService_Stub::Echo, request,
                                                [](const echo::EchoResponse& response) {
                                                    ZMUDUO_LOG_WARNING
                                                        << "RPC result: " << response.text()
                                                        << ", call count: "
                                                        << response.call_count();
                                                });
        });
    }

    void callEchoTwiceInLoop(const std::string& text) {
        client_.callService("EchoService", [text](const std::shared_ptr<RpcChannel>& channel) {
            if (!channel) {
                std::cerr << "Channel is null\n";
                return;
            }

            // 创建stub
            echo::EchoService::Stub stub(channel.get());
            // 创建request
            echo::EchoRequest request;
            request.set_text(text);
            // 创建response
            auto response = std::make_shared<echo::EchoResponse>();

            // 使用 LambdaClosure 封装 Closure（自动释放）
            auto done = LambdaClosure::Create([response]() {
                ZMUDUO_LOG_WARNING << "RPC result: " << response->text()
                                   << ", call count: " << response->call_count();
            });

            stub.EchoTwice(nullptr, &request, response.get(), done);
        });
    }

  private:
    RpcClient client_;
};
#else
class EchoClient : public RpcCallerClient<echo::EchoService_Stub> {
  public:
    EchoClient(EventLoop* loop, const Address::Ptr& registryAddress)
        : RpcCallerClient<echo::EchoService_Stub>(loop, registryAddress) {}

    void callEcho(const std::string& text) {
        echo::EchoRequest request;
        request.set_text(text);

        callServiceMethod<echo::EchoResponse>("EchoService", &echo::EchoService_Stub::Echo, request,
                                              [](const echo::EchoResponse& response) {
                                                  ZMUDUO_LOG_WARNING
                                                      << "Echo response: " << response.text()
                                                      << ", count: " << response.call_count();
                                              });
    }

    void callEchoTwice(const std::string& text) {
        echo::EchoRequest request;
        request.set_text(text);

        callServiceMethod<echo::EchoResponse>("EchoService", &echo::EchoService_Stub::EchoTwice,
                                              request, [](const echo::EchoResponse& response) {
                                                  ZMUDUO_LOG_WARNING
                                                      << "EchoTwice response: " << response.text()
                                                      << ", count: " << response.call_count();
                                              });
    }
};
#endif

int main(int argc, char* argv[]) {
    EventLoop loop;
    auto      registryAddr = IPv4Address::Create("127.0.0.1", 8500);

    EchoClient client(&loop, registryAddr);
    // 调用Echo方法
    client.callEcho("hello");
    client.callEchoTwice("world");
    loop.loop();
    return 0;
}