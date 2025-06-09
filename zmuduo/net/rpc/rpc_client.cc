#include "zmuduo/net/rpc/rpc_client.h"

#include "zmuduo/base/mutex.h"
#include "zmuduo/net/rpc/rpc_codec.h"
#include "zmuduo/net/tcp_client.h"
#include <utility>

namespace zmuduo::net::rpc {

RpcClient::RpcClient(EventLoop* loop, Address::Ptr registryAddress)
    : m_eventLoop(loop), m_registryAddress(std::move(registryAddress)) {}

void RpcClient::callService(const std::string& serviceName, const ChannelCallback& callback) {
    // 1.先尝试从缓存获取
    {
        auto lock = m_rwLock.getReadGuard();
        auto it   = m_serviceCache.find(serviceName);
        if (it != m_serviceCache.end()) {
            m_eventLoop->queueInLoop(
                [&] { callback(std::make_shared<RpcChannel>(m_eventLoop, it->second)); });
            return;
        }
    }

    // 2.异步服务发现
    discoverService(serviceName, [=](const Address::Ptr& addr) {
        if (addr) {
            {
                auto lock                   = m_rwLock.getWriteGuard();
                m_serviceCache[serviceName] = addr;
            }
            callback(std::make_shared<RpcChannel>(m_eventLoop, addr));
        } else {
            callback(nullptr);
        }
    });
}

void RpcClient::discoverService(const std::string& serviceName, const DiscoverCallback& callback) {
    // 创建临时的 TcpClient 用于本次发现
    auto discoverClient = std::make_shared<TcpClient>(m_eventLoop, m_registryAddress,
                                                      "DiscoverClient_" + serviceName);

    // 连接建立回调
    discoverClient->setConnectionCallback(
        [self = discoverClient, serviceName](const TcpConnectionPtr& connection) {
            if (connection->isConnected()) {
                // 连接成功
                RpcMessage message;
                message.set_type(rpc::DISCOVER_REQUEST);
                message.mutable_discover_req()->set_service_name(serviceName);
                RpcCodec::send(connection, message);
            }
        });

    // 设置消息回调
    discoverClient->setMessageCallback([callback](const TcpConnectionPtr& connection,
                                                  Buffer& buffer, const Timestamp& time) {
        // 创建独立的 Codec 用于本次请求
        RpcCodec codec([callback](const TcpConnectionPtr& connection, const RpcMessage& message) {
            Address::Ptr addr;
            if (message.type() == rpc::DISCOVER_RESPONSE && message.status_code() == 0) {
                addr = IPAddress::Create(message.discover_res().endpoint_ip().data(),
                                         message.discover_res().endpoint_port());
            }
            // 主动关闭连接并回调
            connection->shutdown();
            callback(addr);
        });
        codec.onMessage(connection, buffer, time);
    });

    // 开始连接
    discoverClient->connect();
}
}  // namespace zmuduo::net::rpc