#include "zmuduo/net/rpc/rpc_server.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/tcp_client.h"

#include <memory>

namespace zmuduo::net::rpc {

RpcServer::RpcServer(EventLoop* loop, const Address::Ptr& listenAddr)
    : m_eventLoop(loop), m_server(loop, listenAddr, "RpcServer") {
    m_server.setConnectionCallback([this](auto& connection) { onConnection(connection); });
    m_codec = std::make_unique<RpcCodec>(
        [this](const TcpConnectionPtr& connection, const RpcMessage& message) {
            onRpcMessage(connection, message);
        });
    m_server.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& time) {
            m_codec->onMessage(connection, buffer, time);
        });
}

void RpcServer::start() {
    m_server.start();
}

void RpcServer::setRegistryAddress(const Address::Ptr& registryAddress) {
    m_registryAddress = registryAddress;
}

void RpcServer::registerService(google::protobuf::Service* service) {
    auto* descriptor               = service->GetDescriptor();
    m_services[descriptor->name()] = service;

    // 如果设置了注册中心，则向注册中心注册服务
    if (m_registryAddress && !m_registryAddress->toString().empty()) {
        if (!m_client) {
            m_client =
                std::make_unique<TcpClient>(m_eventLoop, m_registryAddress, "RpcServerClient");
        }
        registerToRegistry(descriptor->name());
    }
}

void RpcServer::onConnection(const TcpConnectionPtr& connection) {
    ZMUDUO_LOG_INFO << "RpcServer - " << connection->getPeerAddress() << " -> "
                    << connection->getLocalAddress() << " is "
                    << (connection->isConnected() ? "UP" : "DOWN");
}

void RpcServer::onRpcMessage(const TcpConnectionPtr& connection, const RpcMessage& message) {
    switch (message.type()) {
        case rpc::RPC_REQUEST: handleRpcRequest(connection, message); break;
        case rpc::HEARTBEAT_RESPONSE: handleHeartBeatResponse(connection, message); break;
        default: ZMUDUO_LOG_WARNING << "Unsupported message type: " << message.type();
    }
}

void RpcServer::handleRpcRequest(const TcpConnectionPtr& conn, const RpcMessage& message) {
#define SEND_ERROR_RESPONSE(code, msg)                                                             \
    do {                                                                                           \
        RpcMessage response;                                                                       \
        response.set_type(rpc::RPC_RESPONSE);                                                      \
        response.set_sequence_id(message.sequence_id());                                           \
        response.set_status_code(code);                                                            \
        response.mutable_response()->set_error(msg);                                               \
    } while (false)

    const rpc::RpcRequest& request = message.request();
    // 根据服务名查找对应的服务
    auto it = m_services.find(request.service_name());
    if (it == m_services.end()) {
        // 没找到对应的服务,进行返回
        SEND_ERROR_RESPONSE(1, "Service not found");
        return;
    }
    // 找到了对应的服务
    auto* service = it->second;
    // 查找服务对应的描述符和方法
    auto* descriptor = service->GetDescriptor();
    auto* method     = descriptor->FindMethodByName(request.method_name());
    if (!method) {
        // 方法没找到,返回错误信息
        SEND_ERROR_RESPONSE(2, "Method not found");
        return;
    }

    google::protobuf::Message* request_msg = service->GetRequestPrototype(method).New();
    if (!request_msg->ParseFromString(request.params())) {
        SEND_ERROR_RESPONSE(3, "Invalid request params");
        delete request_msg;
        return;
    }
#undef SEND_ERROR_RESPONSE
    // 创建一个接收响应的消息
    google::protobuf::Message* response_msg = service->GetResponsePrototype(method).New();
    // 调用对应方法
    service->CallMethod(method, nullptr, request_msg, response_msg, nullptr);
    // 设置调用成功的响应并发送
    rpc::RpcMessage response;
    response.set_type(rpc::RPC_RESPONSE);
    response.set_sequence_id(message.sequence_id());
    response.set_status_code(0);
    response.mutable_response()->set_data(response_msg->SerializeAsString());
    RpcCodec::send(conn, response);
    // 回收内存
    delete request_msg;
    delete response_msg;
}

void RpcServer::handleHeartBeatResponse(const TcpConnectionPtr& connection,
                                        const RpcMessage&       message) {
    auto& response = message.heartbeat_res();
    if (!response.healthy()) {
        ZMUDUO_LOG_WARNING << "register server is not safe, because " << response.message();
        // todo:暂时取消心跳定时器并移除服务
        m_services.erase(response.service_id());
        m_eventLoop->cancel(m_timerIds[response.service_id()]);
    }
}

void RpcServer::registerToRegistry(const std::string& serviceName) {
    // 连接成功后的回调
    m_client->setConnectionCallback([this, serviceName](const TcpConnectionPtr& connection) {
        if (connection->isConnected()) {
            // 构建消息
            RpcMessage message;
            message.set_type(rpc::REGISTER_REQUEST);
            auto* registerRequest = message.mutable_register_req();
            // 设置服务名
            registerRequest->set_service_name(serviceName);
            // 设置服务的ip和端口
            registerRequest->set_listen_ip(
                m_server.getIpPort().substr(0, m_server.getIpPort().find(':')));
            registerRequest->set_listen_port(static_cast<uint32_t>(
                std::stoi(m_server.getIpPort().substr(m_server.getIpPort().find(':') + 1))));
            // 发送消息
            RpcCodec::send(connection, message);
        }
    });
    // 接收消息的回调
    m_client->setMessageCallback([this, serviceName](const TcpConnectionPtr& connection,
                                                     Buffer& buffer, const Timestamp& time) {
        // 创建codec用于解析消息
        RpcCodec codec([this, serviceName](const TcpConnectionPtr& connection,
                                           const RpcMessage&       message) {
            if (message.type() == rpc::REGISTER_RESPONSE && message.status_code() == 0) {
                // eventLoop必须创建一个定时器,发送心跳信息
                ZMUDUO_LOG_DEBUG << serviceName << "'s heartbeat interval is "
                                 << message.register_res().heartbeat_interval();
                auto timerId = m_eventLoop->runEvery(
                    message.register_res().heartbeat_interval(),
                    [id = message.register_res().assigned_id(), connection]() {
                        RpcMessage heartMessage;
                        heartMessage.set_type(rpc::HEARTBEAT_REQUEST);
                        auto* heartRequest = heartMessage.mutable_heartbeat_req();
                        heartRequest->set_service_id(id);
                        heartRequest->set_timestamp(Timestamp::Now().getMicroSecondsSinceEpoch());
                        // 发送消息
                        RpcCodec::send(connection, heartMessage);
                    });
                // 注册timerId
                m_timerIds[message.register_res().assigned_id()] = timerId;
            }
            // 主动关闭连接并回调
            // todo:移除关闭,在超时未接收回调中关闭
            connection->shutdown();
        });
        codec.onMessage(connection, buffer, time);
    });
    // 写入完成的回调
    m_client->setWriteCompleteCallback([](auto& connection) { connection->shutdown(); });
    // 连接
    m_client->connect();
}

}  // namespace zmuduo::net::rpc