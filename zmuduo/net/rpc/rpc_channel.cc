#include "zmuduo/net/rpc/rpc_channel.h"

#include "zmuduo/net/rpc/rpc_codec.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/tcp_connection.h"
#include <memory>
#include <utility>

namespace zmuduo::net::rpc {
RpcChannel::RpcChannel(EventLoop* loop, Address::Ptr serverAddress)
    : m_eventLoop(loop), m_serverAddress(std::move(serverAddress)) {}

RpcChannel::~RpcChannel() = default;

void RpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                            google::protobuf::RpcController*          controller,
                            const google::protobuf::Message*          request,
                            google::protobuf::Message*                response,
                            google::protobuf::Closure*                done) {
    // 创建临时client
    auto client = std::make_shared<TcpClient>(m_eventLoop, m_serverAddress,
                                              "RpcClient_" + method->service()->name() + ":" +
                                                  method->name());

    rpc::RpcMessage message;
    message.set_type(rpc::RPC_REQUEST);
    message.set_sequence_id(1);  // TODO: generate sequence id
    // rpc请求
    auto* rpcRequest = message.mutable_request();
    rpcRequest->set_service_name(method->service()->name());
    rpcRequest->set_method_name(method->name());
    rpcRequest->set_params(request->SerializeAsString());

    // 连接成功的回调
    client->setConnectionCallback([message, self = client](const TcpConnectionPtr& connection) {
        if (connection->isConnected()) {
            RpcCodec::send(connection, message);
        }
    });
    // 创建一个解码器,当收到消息后进行
    RpcCodec codec([=](const TcpConnectionPtr& connection, const RpcMessage& message) {
        if (message.type() == rpc::RPC_RESPONSE) {
            if (message.status_code() == 0) {
                // 响应成功
                response->ParseFromString(message.response().data());
            } else if (controller) {
                controller->SetFailed(message.response().error());
            }
            connection->shutdown();
            // 响应完成后检测是否需要执行done函数
            if (done) {
                done->Run();
            }
        }
    });
    // 设置client的消息回调
    client->setMessageCallback(
        [=](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& time) mutable {
            codec.onMessage(connection, buffer, time);
        });
    // 连接指定服务器
    client->connect();
}
}  // namespace zmuduo::net::rpc