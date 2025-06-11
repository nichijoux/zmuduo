#include <openssl/ssl.h>
#include <zmuduo/base/logger.h>
#include <zmuduo/net/tcp_server.h>

using namespace zmuduo;
using namespace zmuduo::net;

class EchoServer {
  public:
    EchoServer(EventLoop* loop, const Address::Ptr& addr, const std::string& name)
        : server_(loop, addr, name), loop_(loop) {
        // 注册回调函数
        server_.setConnectionCallback(defaultConnectionCallback);

        server_.setMessageCallback([this](auto&& PH1, auto&& PH2, auto&& PH3) {
            onMessage(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                      std::forward<decltype(PH3)>(PH3));
        });
        // 加载证书
        if (server_.loadCertificates("cacert.pem", "privkey.pem")) {
            ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
        } else {
            ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
        }
        // 设置合适的loop线程数量 loopThread
        server_.setThreadNum(1);
    }
    void start() {
        server_.start();
    }

  private:
    // 可读写事件回调
    void onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp& time) {
        std::string msg = buf.retrieveAllAsString();
        ZMUDUO_LOG_FMT_IMPORTANT("收到消息 %s", msg.c_str());
        conn->send("You said: " + msg);
        conn->shutdown();  // 写端   EPOLLHUP => closeCallback_
    }

    EventLoop* loop_;
    TcpServer  server_;
};

int main() {
    EventLoop  loop;
    auto       addr = IPv4Address::Create("127.0.0.1", 8000);
    EchoServer server(&loop, addr, "SSLServer");
    server.start();
    loop.loop();
}