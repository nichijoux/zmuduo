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

### 4. `zmuduo::net::http` - HTTP 请求分发模块

该模块提供 HTTP 请求分发功能，用于构建高性能 HTTP 服务器。

- 基于 URL 和 HTTP 方法的请求分发，支持精确匹配和通配符匹配。
- 拦截器（`HttpInterceptor`）和过滤器（`HttpFilter`）链，用于请求验证和处理。
- 灵活的 Servlet 架构，支持动态回调。
- HttpClient 和 HttpServer，支持服务器和客户端两种形式
- Http协议解析器，可解析HTTP1.0及HTTP1.1协议

### 5. `zmuduo::net::ws` - WebSocket 协议模块

该模块提供 WebSocket 协议支持，用于实现实时双向通信。

- WebSocket 帧解析和生成，处理协议头和数据帧。
- Servlet 风格的 WebSocket 请求分发，支持动态消息处理。
- 客户端和服务器端 WebSocket 连接管理。
- 基于基类的子协议的扩展

### 6. `zmuduo::net::email` - EMail 协议模块

该模块提供 Smtp 和 Pop3 协议支持，用于发送和下载邮件

- 邮件协议的发送和下载
- Smtp 客户端
- Pop3 客户端

## example

- `example` 目录中是 zmuduo 库的基础使用，其中部分代码(如`pingpong`)参考 [muduo](https://github.com/chenshuo/muduo) 实现。
- `webdav` 目录实现了一个简单的 webdav 服务器，支持 OPTIONS、GET、PUT、DELETE、MKCOL等命令，但不支持 LOCK 和 UNLOCK

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

zmuduo
的各模块通过头文件提供接口，详细用法请参考头文件中的注释
）或项目文档。每个模块的类和方法均附有 Doxygen 格式的注释，描述功能、参数和注意事项。

## 依赖项

- 如果启用 `ZMUDUO_ENABLE_OPENSSL` 则项目将依赖 openssl
- `example/inspect` 样例中使用了 `Gperftools`
- `example/webdav` 样例中如果找到 `zlib`，则代码中会启用压缩过滤器，该过滤器将在servlet代码处理完毕后进行gzip或者deflate压缩

## 许可证

zmuduo 采用 [BSD 3-Clause 许可证](LICENSE)。

## 致谢

- 本项目基于 [muduo](https://github.com/chenshuo/muduo) 修改。
- 本项目部分代码（如Address.h/.cc）参考项目 [sylar](https://github.com/sylar-yin/sylar/) 实现