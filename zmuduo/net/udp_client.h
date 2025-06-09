// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_UDP_CLIENT_H
#define ZMUDUO_NET_UDP_CLIENT_H

#include "zmuduo/net/address.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/socket.h"
#include <functional>
#include <memory>
#include <string>

namespace zmuduo::net {

/**
 * @class UdpClient
 * @brief UDP 客户端类，用于异步发送和接收 UDP 数据。
 *
 * UdpClient 通过事件驱动模型（EventLoop）实现非阻塞 UDP
 * 通信，支持向指定服务器地址发送数据并接收响应。 用户可通过 MessageCallback 自定义消息处理逻辑。
 *
 * @note UdpClient 不可拷贝（继承自 NoCopyable），线程安全，但 MessageCallback 需用户确保线程安全。
 * @note 发送操作通过 runInLoop 确保在事件循环线程中执行。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/udp_client.h"
 * #include <iostream>
 *
 * int main() {
 *     EventLoop loop;
 *     auto serverAddr = IPv4Address::Create("127.0.0.1", 8888);
 *     UdpClient client(&loop, serverAddr, AF_INET, "TestClient");
 *     client.setMessageCallback([](UdpClient& client, Buffer& buffer) {
 *         std::cout << "Received: " << buffer.retrieveAsString() << std::endl;
 *     });
 *     client.start();
 *     client.send("Hello, Server!", 13);
 *     loop.loop();
 *     return 0;
 * }
 * @endcode
 */
class UdpClient : NoCopyable {
  public:
    /**
     * @brief 消息回调类型
     * @typedef std::function&lt;void(UdpClient& client, Buffer& inBuffer)&gt;
     * @param[in] client Udp客户端
     * @param[in] buffer 接收到的消息缓冲区
     */
    using MessageCallback =
        std::function<void(UdpClient& client, Buffer& buffer)>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] serverAddress 服务器地址。
     * @param[in] domain 地址域（AF_INET 或 AF_INET6）。
     * @param[in] name 客户端名称，用于日志标识。
     */
    UdpClient(EventLoop* loop, Address::Ptr serverAddress, int domain, std::string name);

    /**
     * @brief 析构函数。
     * @note 禁用事件监听并移除通道。
     */
    ~UdpClient();

    /**
     * @brief 启动客户端，启用读事件监听。
     */
    void start();

    /**
     * @brief 停止客户端，禁用所有事件监听。
     */
    void stop();

    /**
     * @brief 发送数据到服务器。
     * @param[in] data 数据指针。
     * @param[in] length 数据长度。
     * @note 异步调用 sendInLoop，确保线程安全。
     */
    void send(const void* data, size_t length);

    /**
     * @brief 发送字符串数据到服务器。
     * @param[in] data 字符串数据。
     * @note 委托给 send(const void*, size_t)。
     */
    void send(const std::string& data) {
        send(data.c_str(), data.size());
    }

    /**
     * @brief 设置消息回调函数。
     * @param[in] callback 消息处理回调。
     * @note 回调在事件循环线程中执行，需确保线程安全。
     */
    void setMessageCallback(MessageCallback callback) {
        m_messageCallback = std::move(callback);
    }

  private:
    /**
     * @brief 在事件循环线程中发送数据。
     * @param[in] message 待发送的数据指针。
     * @param[in] length 数据长度。
     * @note 使用 sendto 发送数据到服务器地址。
     */
    void sendInLoop(const void* message, size_t length);

    /**
     * @brief 处理读事件，接收服务器数据。
     * @note 使用 recvfrom 接收数据，验证服务器地址，触发 MessageCallback。
     */
    void handleRead();

  private:
    EventLoop*               m_eventLoop;        ///< 事件循环
    Address::Ptr             m_serverAddress;    ///< 服务器地址
    std::string              m_name;             ///< 客户端名称
    UdpSocket                m_socket;           ///< UDP 套接字
    std::unique_ptr<Channel> m_channel;          ///< 事件通道
    Buffer                   m_inputBuffer;      ///< 输入缓冲区
    MessageCallback          m_messageCallback;  ///< 消息回调
};

}  // namespace zmuduo::net

#endif