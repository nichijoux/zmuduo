#ifndef ZMUDUO_NET_RPC_RPC_SERVER_H
#define ZMUDUO_NET_RPC_RPC_SERVER_H

#include "zmuduo/net/rpc/rpc_codec.h"
#include "zmuduo/net/rpc/rpc_core.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/tcp_server.h"

namespace zmuduo::net::rpc {
/**
 * @class RpcServer
 * @brief RPC 服务端，处理客户端 RPC 请求并向注册中心注册服务。
 *
 * RpcServer 基于 TcpServer 监听客户端连接，处理 RPC_REQUEST 消息，调用注册的服务方法。
 * 支持向注册中心（RegistryServer）注册服务，使用 TcpClient 发送 REGISTER_REQUEST。
 * 服务信息存储在 m_services 映射中，键为服务名，值为 Protobuf Service 对象。
 *
 * @note RpcServer 线程安全，操作需在 EventLoop 线程中执行，RpcCodec 回调需用户确保线程安全。
 * @note 注册中心连接未处理响应，可能需确认注册成功。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/rpc_server.h"
 * #include <my_service.pb.h>
 * zmuduo::net::EventLoop loop;
 * auto listenAddr = zmuduo::net::Address::Create("0.0.0.0", 8888);
 * auto registryAddr = zmuduo::net::Address::Create("127.0.0.1", 9999);
 * zmuduo::net::rpc::RpcServer server(&loop, listenAddr);
 * server.setRegistryAddress(registryAddr);
 * MyServiceImpl serviceImpl;
 * server.registerService(&serviceImpl);
 * server.start();
 * loop.loop();
 * @endcode
 */
class RpcServer {
  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] listenAddress 监听地址。
     * @note 初始化 TcpServer 和 RpcCodec，设置连接和消息回调。
     */
    RpcServer(EventLoop* loop, const Address::Ptr& listenAddress);

    /**
     * @brief 启动 RPC 服务端。
     * @note 调用 TcpServer::start，开始监听客户端连接。
     */
    void start();

    /**
     * @brief 注册 Protobuf 服务。
     * @param[in] service Protobuf 服务指针。
     * @note 将服务添加到 m_services，若设置注册中心则触发注册。
     */
    void registerService(google::protobuf::Service* service);

    /**
     * @brief 设置注册中心地址。
     * @param[in] registryAddress 注册中心地址。
     * @note 用于后续服务注册，需在 registerService 前调用。
     */
    void setRegistryAddress(const Address::Ptr& registryAddress);

  private:
    /**
     * @brief 处理连接事件。
     * @param[in] connection TCP 连接智能指针。
     * @note 记录连接建立或断开的日志。
     */
    static void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 处理 RPC 消息。
     * @param[in] connection TCP 连接指针。
     * @param[in] message 解析后的 RpcMessage。
     * @note 处理 RPC_REQUEST 类型消息，不支持的类型记录警告日志。
     */
    void onRpcMessage(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 处理 RPC 请求。
     * @param[in] connection TCP 连接。
     * @param[in] message 包含 RPC_REQUEST 的消息。
     * @note 查找服务和方法，调用服务，构造响应，处理错误情况。
     */
    void handleRpcRequest(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 处理心跳响应
     * @param connection TCP 连接。
     * @param message 包含 HEARTBEAT_RESPONSE 的消息。
     * @note 服务器每n秒发送一次心跳数据
     */
    void handleHeartBeatResponse(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 向注册中心注册服务。
     * @param[in] serviceName 服务名称。
     * @note 使用 TcpClient 发送 REGISTER_REQUEST，完成后关闭连接。
     */
    void registerToRegistry(const std::string& serviceName);

  private:
    EventLoop*                 m_eventLoop;        ///< 事件循环实例
    TcpServer                  m_server;           ///< TCP 服务端实例
    std::unique_ptr<TcpClient> m_client;           ///< TCP 客户端，连接注册中心
    Address::Ptr               m_registryAddress;  ///< 注册中心地址
    std::unique_ptr<RpcCodec>  m_codec;            ///< RPC 消息编解码器
    std::unordered_map<std::string, google::protobuf::Service*>
                                                 m_services;  ///< 服务名到 Service 的映射
    std::unordered_map<std::string, TimerId> m_timerIds;  ///< 分配id 到 定时器id 的映射
};
}  // namespace zmuduo::net::rpc

#endif