# zmuduo

**zmuduo** 是一个高性能的 C++ 网络库，基于 [muduo](https://github.com/chenshuo/muduo) 修改和扩展，旨在为构建可扩展的网络应用提供支持。

项目采用现代 C++17 特性，基于事件驱动架构。

支持 TCP、UDP 通信、HTTP 请求分发、WebSocket 协议和 SMTP、POP3协议。

本项目在 `muduo` 的基础上进行了功能增强和模块化优化，适用于高并发网络服务和实时通信场景。

## 项目简介

zmuduo 通过模块化的命名空间设计，提供了网络通信和实用工具的功能。

- 包括 HTTP 服务、UDP 客户端/服务器、WebSocket 支持、文件系统操作和基础工具。
- 项目继承了 muduo 的事件驱动和非阻塞 I/O 设计，同时增加了新的功能模块，如 HTTP Servlet 分发和
  WebSocket 协议支持。

## 模块描述

### 1. `zmuduo::base` - 基础工具模块

该模块提供 zmuduo 的核心基础工具，为其他模块提供支持。

- 用于禁止对象拷贝或移动的基类。
- 支持多级别多色彩的日志记录，线程安全、支持流式或格式化输出。
- 时间戳，用于定时器和记录消息时间

> 日志使用方式
```c++
ZMUDUO_LOG_INFO << "Hello World";
ZMUDUO_LOG_FMT_INFO("%s","hello world");
```

### 2. `zmuduo::utils` - 实用工具模块

该模块提供一些工具类

- 文件系统操作工具 `FSUtil`
- 字符串操作工具 `StringUtil`
- 通用工具 `CommonUtil`
- 哈希工具 `HashUtil`
- 系统工具 `SystemUtil`

### 3. `zmuduo::net` - 网络通信模块

该模块提供异步网络通信功能，包括 TCP、UDP以及串口通信，基于事件驱动模型实现高效 I/O 处理。

- 支持 TCP、UDP 以及串口通信
- 可通过环境变量使用 `epoll`、`poll`和 `select` 三种 `IO` 事件模型
- 基于 EventLoop、Acceptor、Channel的事件驱动模型

> TCP 服务端使用示例
```c++
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
```

> TCP 客户端使用示例

```c++
#include <zmuduo/base/logger.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/tcp_client.h>

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      addr = IPv4Address::Create("127.0.0.1", 8000);
    TcpClient client(&loop, addr, "SSLClient");
    if (client.createSSLContext() && client.loadCustomCACertificate("cacert.pem", "")) {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
    } else {
        ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
    }
    client.setConnectionCallback(
        [](const auto& connection) { connection->send("I'm a ssl client"); });
    client.setMessageCallback(
        [](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp&) {
            auto msg = buffer.retrieveAllAsString();
            ZMUDUO_LOG_FMT_IMPORTANT("收到消息 %s", msg.c_str());
            connection->send(msg);
        });

    client.connect();
    loop.loop();
}
```

> UDP 服务端使用示例
```c++
#include "zmuduo/base/logger.h"
#include "zmuduo/net/udp_server.h"

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      listenAddress = IPv4Address::Create("127.0.0.1", 8000);
    UdpServer server(&loop, listenAddress, "UdpServerTest");
    server.setMessageCallback(
        [](UdpServer& server, Buffer& buffer, const Address::Ptr& peerAddress) {
            auto message = buffer.retrieveAllAsString();
            ZMUDUO_LOG_INFO << "receive message: " << message << "from " << peerAddress;
            server.send("You said: " + message, peerAddress);
        });
    server.start();
    loop.loop();
    return 0;
}
```

> UDP 客户端使用示例
```c++
#include "zmuduo/base/logger.h"
#include "zmuduo/net/timer_queue.h"
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/udp_client.h>

using namespace zmuduo;
using namespace zmuduo::net;

int main() {
    EventLoop loop;
    auto      serverAddress = IPv4Address::Create("127.0.0.1", 8000);
    UdpClient client(&loop, serverAddress, AF_INET, "UdpClient");
    // 启动client
    client.start();
    client.setMessageCallback([](auto& client, auto& inputBuffer) {
        ZMUDUO_LOG_IMPORTANT << inputBuffer.retrieveAllAsString();
    });
    // equals
    // loop.runEvery(2.5, [&client]() { client.send(Timestamp::Now().toString()); });
    TimerQueue timerQueue(&loop);
    timerQueue.addTimer([&client]() { client.send(Timestamp::Now().toString()); },
                        Timestamp::Now(), 2.5);
    loop.loop();
    return 0;
}
```

> 串口使用示例
```c++
EventLoop  loop;
SerialPort serialPort(&loop, "/dev/pts/3");
serialPort.setOpenedCallback([&serialPort](bool opened) {
    if (opened) {
        ZMUDUO_LOG_IMPORTANT << "打开串口成功";
        serialPort.send("hello world");
    }
});
serialPort.setMessageCallback([](SerialPort& port, Buffer& in) {
    auto s = in.retrieveAllAsString();
    ZMUDUO_LOG_IMPORTANT << s << "|" << toHex(s);
    port.send("you said: " + s);
});
serialPort.open();
loop.loop();
```

### 4. `zmuduo::net::http` - HTTP 协议实现模块

该模块提供 HTTP 协议支持，用于构建高性能 HTTP 服务器。

- 基于 URL 和 HTTP 方法的请求分发，支持精确匹配和通配符匹配。
- 拦截器（`HttpInterceptor`）和过滤器（`HttpFilter`）链，用于请求验证和处理。
- 灵活的 `Servlet` 架构，支持动态回调。
- `HttpClient` 和 `HttpServer`，支持服务器和客户端两种形式
- Http协议解析器，可解析HTTP1.0及HTTP1.1协议

> 服务端使用示例：
```c++
EventLoop  loop;
auto       address = IPv4Address::Create("127.0.0.1", 8000);
HttpServer server(&loop, address, "HttpServer", true);
auto&      dispatcher = server.getServletDispatcher();
dispatcher.addExactServlet("/hello", [](const HttpRequest& request, HttpResponse& response) {
    ZMUDUO_LOG_IMPORTANT << "收到请求:\n" << request;
    response.setStatus(HttpStatus::OK);
    response.setBody("hello");
});
dispatcher.addWildcardServlet(
    "/hello/*", [](const HttpRequest& request, HttpResponse& response) {
        ZMUDUO_LOG_IMPORTANT << "收到请求:\n" << request;
        response.setStatus(HttpStatus::OK);
        response.setBody("hello the world\nthe query is " + request.getQuery() +
                         "\n the body is " + request.getBody());
    });
server.start();
loop.loop();
```

> 客户端使用示例
```c++
EventLoop loop;
auto      address = IPv4Address::Create("127.0.0.1", 8000);
ZMUDUO_LOG_IMPORTANT << address->toString();
HttpClient client(&loop, address, "client");
client.doGet("/hello", [](const auto& ptr) {
    if (ptr) {
        ZMUDUO_LOG_IMPORTANT << ptr->toString();
    }
});
client.doGet(
    "/hello/2",
    [&loop](const auto& ptr) {
        if (ptr) {
            ZMUDUO_LOG_IMPORTANT << ptr->toString();
        }
        loop.quit();
    },
    {}, "", 5);
loop.loop();
```

### 5. `zmuduo::net::ws` - WebSocket 协议模块

该模块提供 WebSocket 协议支持，用于实现实时双向通信。

- WebSocket 帧解析和生成，处理协议头和数据帧。
- Servlet 风格的 WebSocket 请求分发，支持动态消息处理。
- 客户端和服务器端 WebSocket 连接管理。
- 基于基类的子协议的扩展

> 服务器子协议使用示例
```c++
EventLoop    loop;
Address::Ptr address = IPv4Address::Create("127.0.0.1", 8000);
WSServer     server(&loop, address, "WSServer[OpenSSL]");
// 加载证书
if (server.loadCertificates("cacert.pem", "privkey.pem")) {
    ZMUDUO_LOG_FMT_IMPORTANT("加载证书成功");
} else {
    ZMUDUO_LOG_FMT_IMPORTANT("加载证书失败");
}
server.getServletDispatcher().addExactServlet("/echo", [](const WSFrameMessage&   message,
                                                          const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_INFO << "received: " << message.m_payload;
    connection->send(WSFrameMessage(WSFrameHead::TEXT_FRAME, "You said:" + message.m_payload)
                         .serialize(false));
});
server.getServletDispatcher().addExactServlet("/", [](const WSFrameMessage&   message,
                                                      const TcpConnectionPtr& connection) {
    connection->send(WSFrameMessage::MakeCloseFrame(WSCloseCode::NORMAL_CLOSURE, "我就想关闭")
                         .serialize(false));
});
server.start();
loop.loop();
```

> 客户端子协议使用示例
```c++
EventLoop loop;
WSClient  client(&loop, "wss://127.0.0.1:8000/echo", "WSClient[SubProtocol]");
client.addSupportSubProtocol(std::make_shared<JsonWSSubProtocol>());
// 成功握手的回调
client.setWSConnectionCallback([&](bool connect) {
    if (connect) {
        ZMUDUO_LOG_IMPORTANT << "连接成功";
        Person person{"Ned Flanders", "744 Evergreen Terrace", 60};
        JSON   j = person;
        client.sendWSFrameMessage(WSFrameHead::BIN_FRAME, to_string(j));
    } else {
        ZMUDUO_LOG_IMPORTANT << "断开连接";
    }
});
// 接收数据的回调
client.setWSMessageCallback(
    [](const TcpConnectionPtr& connection, const WSFrameMessage& message) {
        if (message.m_subProtocol) {
            ZMUDUO_LOG_IMPORTANT
                << "收到数据:"
                << std::any_cast<JSON>(message.m_subProtocol->process(message.m_payload));
        }
    });
client.connect();
loop.loop();
```

### 6. `zmuduo::net::email` - EMail 协议模块

该模块提供 Smtp 和 Pop3 协议支持，用于发送和下载邮件

- 邮件协议的发送和下载
- Smtp 客户端
- Pop3 客户端

> Smtp 使用示例
```c++
EventLoop  loop;
auto       address = IPv4Address::Create("127.0.0.1", 1025);
SmtpClient client(&loop, address, "SmtpClient");
client.setFailureCallback(
    [](const std::string& message) { ZMUDUO_LOG_FMT_ERROR("发送失败%s", message.c_str()); });
client.setSuccessCallback([]() { ZMUDUO_LOG_FMT_INFO("发送成功"); });
EMail::Ptr email =
    EMail::Create("user1@example.com", "password1", "Test Email",
                  "<h1>Hello from SMTP client!</h1>", {"to@example.com"}, {}, {});
auto entity = EMailEntity::CreateAttachment("attachment");
email->addEntity(entity);
client.send(email);

loop.loop();
```

> Pop3 使用示例
```c++
EventLoop  loop;
auto       address = IPv4Address::Create("127.0.0.1", 110);
Pop3Client client(&loop, address, "user", "pass");
client.setAuthenticateCallback([&]() {
    client.stat([](const Pop3StatResponse::Ptr& response) {
        if (response->m_success) {
            ZMUDUO_LOG_INFO << response->m_num;
            ZMUDUO_LOG_INFO << response->m_size;
        } else {
            ZMUDUO_LOG_INFO << response;
        }
    });
    client.list([](const Pop3ListResponse::Ptr& response) {
        if (response->m_success) {
            for (const auto& e : response->m_entries) {
                ZMUDUO_LOG_INFO << e.m_num << "," << e.m_size;
            }
        }
    });
    client.retr(5, [](const Pop3RetrResponse::Ptr& response) {
        if (response->m_success) {
            ZMUDUO_LOG_IMPORTANT << response->m_message;
        }
    });
});
client.connect();
loop.loop();
```

### 7. `zmuduo::net::rpc` - RPC 远程调用模块

该模块基于 `protobuf` 编写，提供了服务注册、发现和远程调用功能。

- 异步 RPC 调用，通过回调封装 Protobuf 服务，和模板功能实现 RPC 回调。
- 服务注册（ServiceRegistration）和服务发现（ServiceDiscovery），与注册中心交互。
- 心跳机制（HeartbeatSignal），确保服务存活状态。
- 统一的 RPC 消息结构（RpcMessage），支持多种消息类型。

> 详细使用示例见 `example/rpc_test`
- 其中 echo_registry_server 为注册中心 
- echo_server 为服务提供方 
- echo_client 为服务调用方

> 调用方代码如下
```c++
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
```

## example

- `example` 目录中是 zmuduo 库的基础使用，其中部分代码(如`pingpong`)参考 [muduo](https://github.com/chenshuo/muduo) 实现。
- `webdav` 目录实现了一个简单的 webdav 服务器，支持 OPTIONS、GET、PUT、DELETE、MKCOL等命令，但不支持 LOCK 和 UNLOCK
- `rpc` 目录展示了简单的 rpc 调用方式
- `http_test` 目录展示了几种简单的 `http` 客户端和服务端使用方式，如带 `openssl` 的客户端访问**百度首页**
- `websocket` 目录展示了三种使用websocket的方式，分别为 **基础websocket**、**带OpenSSL的websocket**、**使用自定义子协议的WebSocket**

## 安装说明

### 依赖要求

- C++17 兼容的编译器（如 GCC 8+、Clang 6+、MSVC 2017+）。
- POSIX 兼容系统（Linux、macOS），用于网络功能。
- 标准 C++ 库，支持 `std::filesystem`。
- 可选：CMake 用于项目构建。

### 构建步骤

1. 克隆仓库：
   ```bash
   git clone https://github.com/nichijoux/zmuduo.git
   cd zmuduo
   ```
2. 创建构建目录：
   ```bash
   mkdir build && cd build
   ```

3. 配置并编译：
   ```bash
   cmake ..
   make
   ```
4. 安装（可选）：
   ```bash
   sudo make install
   ```
   或
   ```bash
   chmod +x build.sh && ./build.sh
   ```

## 使用说明

`zmuduo` 的各模块通过头文件提供接口，详细用法请参考头文件中的注释或项目文档。

每个模块的类和方法均附有 `Doxygen` 格式的注释，描述功能、参数和注意事项。

## 依赖项

- 如果启用 `ZMUDUO_ENABLE_OPENSSL` 则项目将依赖 `openssl`
- `example/inspect` 样例中使用了 `Gperftools`
- `example/webdav` 样例中如果找到 `zlib`，则代码中会启用压缩过滤器，该过滤器将在servlet代码处理完毕后进行gzip或者deflate压缩
- `example/websocket` 样例中自定义子协议行为使用了 `nlohmann::json`
- 如果找到了 `protobuf` 则项目将编译 `rpc` 模块

## 许可证

zmuduo 采用 [BSD 3-Clause 许可证](LICENSE)。

## 致谢

- 本项目基于 [muduo](https://github.com/chenshuo/muduo) 修改。
- 本项目部分代码（如Address.h/.cc）参考项目 [sylar](https://github.com/sylar-yin/sylar/) 实现
- 本项目使用了 [OpenSSL](https://www.openssl.org/) 提供的加密库实现 SSL/TLS 功能
- `example/websocket` 中自定义json子协议使用了[nlohmann::json](https://github.com/nlohmann/json)