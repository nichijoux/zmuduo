#include "zmuduo/net/rpc/registry_server.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/net/rpc/rpc_codec.h"
#include <memory>

namespace zmuduo::net::rpc {
RegistryServer::RegistryServer(EventLoop* loop, const Address::Ptr& listenAddr)
    : m_server(loop, listenAddr, "RegistryServer"), m_interval(30) {
    m_server.setConnectionCallback(defaultConnectionCallback);

    m_codec = std::make_unique<RpcCodec>(
        [this](const TcpConnectionPtr& connection, const RpcMessage& message) {
            onRpcMessage(connection, message);
        });
    m_server.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& time) {
            m_codec->onMessage(connection, buffer, time);
        });
}

void RegistryServer::start() {
    m_server.start();
}

void RegistryServer::onRpcMessage(const TcpConnectionPtr& connection, const RpcMessage& message) {
    switch (message.type()) {
        case rpc::REGISTER_REQUEST: handleRegister(connection, message); break;
        case rpc::DISCOVER_REQUEST: handleDiscover(connection, message); break;
        case rpc::HEARTBEAT_REQUEST: handleHeartbeat(connection, message); break;
        default: ZMUDUO_LOG_WARNING << "Unsupported message type: " << message.type();
    }
}

void RegistryServer::handleRegister(const TcpConnectionPtr& connection, const RpcMessage& message) {
    const ServiceRegistration& req = message.register_req();
    m_services[req.service_name()] = {
        req.listen_ip(), req.listen_port(),
        static_cast<uint64_t>(Timestamp::Now().getMicroSecondsSinceEpoch())};
    ZMUDUO_LOG_FMT_INFO("[%s:%zu] register a service [%s]", req.listen_ip().c_str(),
                        req.listen_port(), req.service_name().c_str());
    RpcMessage response;
    response.set_status_code(0);
    response.set_type(rpc::REGISTER_RESPONSE);
    response.set_sequence_id(message.sequence_id());
    auto* result = response.mutable_register_res();
    // 设置id和要求的心跳间隔
    result->set_assigned_id(req.service_name() + "@" + req.listen_ip() + ":" +
                            std::to_string(req.listen_port()));
    result->set_heartbeat_interval(m_interval);
    // 发送消息
    RpcCodec::send(connection, response);
}

void RegistryServer::handleDiscover(const TcpConnectionPtr& connection, const RpcMessage& message) {
    const ServiceDiscovery& req = message.discover_req();
    auto                    it  = m_services.find(req.service_name());
    // 响应
    RpcMessage response;
    response.set_type(rpc::DISCOVER_RESPONSE);
    response.set_sequence_id(message.sequence_id());
    auto* discoverRes = response.mutable_discover_res();
    ZMUDUO_LOG_INFO << connection->getPeerAddress() << "want a service [" << req.service_name()
                    << "]";
    if (it != m_services.end()) {
        // 如果找到了对应的服务
        response.set_status_code(0);
        discoverRes->set_service_name(it->first);
        discoverRes->set_endpoint_ip(it->second.ip);
        discoverRes->set_endpoint_port(it->second.port);
    } else {
        response.set_status_code(1);
    }
    // 发送发现响应
    RpcCodec::send(connection, response);
}

void RegistryServer::handleHeartbeat(const TcpConnectionPtr& connection,
                                     const rpc::RpcMessage&  message) {
    const rpc::HeartbeatSignal& req = message.heartbeat_req();
    // 查找对应服务
    auto it = m_services.find(req.service_id());
    if (it == nullptr) {
        ZMUDUO_LOG_WARNING << req.service_id() << " heartbeat but it not registered";
        return;
    }
    // 更新心跳时间
    it->second.heartbeat = Timestamp::Now().getMicroSecondsSinceEpoch();
    // 心跳包响应
    rpc::RpcMessage response;
    response.set_type(rpc::HEARTBEAT_RESPONSE);
    response.set_sequence_id(message.sequence_id());
    auto* res = response.mutable_heartbeat_res();
    res->set_healthy(true);
    res->set_message("OK");
    // 发送消息
    RpcCodec::send(connection, response);
}
}  // namespace zmuduo::net::rpc