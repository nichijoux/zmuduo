#ifndef ZMUDUO_NET_RPC_RPC_CHANNEL_H
#define ZMUDUO_NET_RPC_RPC_CHANNEL_H

#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"
#include <google/protobuf/service.h>

namespace zmuduo::net::rpc {
/**
 * @class RpcChannel
 * @brief Protobuf RPC 通道实现，负责客户端与服务器的 RPC 通信。
 *
 * RpcChannel 继承自 google::protobuf::RpcChannel，基于 TcpClient 和 RpcCodec 实现异步 RPC 调用。
 * 它创建 TCP 连接，发送 RpcMessage 请求，处理响应，并触发回调。
 *
 * @note RpcChannel 线程安全，操作需在 EventLoop 线程中执行，Closure 回调需用户确保线程安全。
 * @note 每次 CallMethod 创建新的 TcpClient，可能影响性能，建议复用连接。
 *
 * @example
 * @code
 * #include "zmuduo/net/rpc/rpc_channel.h"
 * #include <my_service.pb.h>
 * zmuduo::net::EventLoop loop;
 * auto addr = zmuduo::net::IPv4Address::Create("127.0.0.1", 8888);
 * auto channel = std::make_shared<zmuduo::net::rpc::RpcChannel>(&loop, addr);
 * MyService_Stub stub(channel.get());
 * MyRequest request;
 * MyResponse response;
 * stub.MyMethod(nullptr, &request, &response, nullptr);
 * loop.loop();
 * @endcode
 */
class RpcChannel : public google::protobuf::RpcChannel {
  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] serverAddress 服务器地址。
     * @note 初始化事件循环和服务器地址，用于后续 TCP 连接。
     */
    RpcChannel(EventLoop* loop, Address::Ptr serverAddress);

    /**
     * @brief 析构函数。
     * @note 无需特殊清理，TcpClient 由 shared_ptr 管理。
     */
    ~RpcChannel() override;

    /**
     * @brief 执行异步 RPC 调用。
     * @param[in] method 方法描述符，包含服务和方法名称。
     * @param[in] controller RPC 控制器，用于错误处理（可为空）。
     * @param[in] request 请求消息。
     * @param[in,out] response 响应消息，存储调用结果。
     * @param[in] done 完成回调（可为空）。
     * @note 创建临时 TcpClient，发送 RpcMessage 请求，处理响应并触发回调。
     * @note 序列号目前硬编码为 1，需改进为动态生成。
     */
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                    google::protobuf::RpcController*          controller,
                    const google::protobuf::Message*          request,
                    google::protobuf::Message*                response,
                    google::protobuf::Closure*                done) override;

  private:
    EventLoop*   m_eventLoop;      ///< 事件循环指针
    Address::Ptr m_serverAddress;  ///< 服务器地址
};
}  // namespace zmuduo::net::rpc

#endif