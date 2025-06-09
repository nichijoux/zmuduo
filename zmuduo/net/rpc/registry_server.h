#ifndef ZMUDUO_NET_RPC_REGISTER_SERVER_H
#define ZMUDUO_NET_RPC_REGISTER_SERVER_H

#include "zmuduo/net/rpc/rpc_codec.h"
#include "zmuduo/net/rpc/rpc_core.h"
#include "zmuduo/net/tcp_server.h"
#include <unordered_map>

namespace zmuduo::net::rpc {

/**
 * @struct ServiceInfo
 * @brief 服务实例信息结构体。
 *
 * 存储服务实例的 IP 地址、端口号和最后心跳时间，用于服务注册和保活。
 */
struct ServiceInfo {
    std::string ip;         ///< 服务监听的 IP 地址
    uint32_t    port;       ///< 服务监听的端口号
    uint64_t    heartbeat;  ///< 最后心跳时间（微秒时间戳）

    bool operator()(const ServiceInfo& a, const ServiceInfo& b) const {
        return a.heartbeat > b.heartbeat;  // 小的在前
    }
};

// 比较器：让最小的 heartbeat 在堆顶（即“最老的”服务）
struct CompareHeartbeat {
    bool operator()(const ServiceInfo& a, const ServiceInfo& b) const {
        return a.heartbeat > b.heartbeat;  // 小的在前
    }
};
/**
 * @class RegistryServer
 * @brief RPC 服务注册中心，管理服务注册、发现和心跳。
 *
 * RegistryServer 基于 TcpServer 监听客户端连接，处理服务注册（REGISTER_REQUEST）、
 * 服务发现（DISCOVER_REQUEST）和心跳（HEARTBEAT_REQUEST）请求。使用 RpcCodec 编解码
 * RpcMessage，维护服务信息映射。
 *
 * @note RegistryServer 线程安全，操作需在 EventLoop 线程中执行，RpcCodec 回调需用户确保线程安全。
 * @note 未实现服务过期清理机制，可能导致服务信息无限增长。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/registry_server.h"
 * zmuduo::net::EventLoop loop;
 * auto addr = zmuduo::net::IPv4Address::Create("0.0.0.0", 8888);
 * zmuduo::net::rpc::RegistryServer server(&loop, addr);
 * server.start();
 * loop.loop();
 * @endcode
 */
class RegistryServer {
  public:
    using ServiceHeartbeatQueue =
        std::priority_queue<ServiceInfo, std::vector<ServiceInfo>, CompareHeartbeat>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] listenAddress 监听地址。
     * @note 初始化 TcpServer 和 RpcCodec，设置连接和消息回调。
     */
    RegistryServer(EventLoop* loop, const Address::Ptr& listenAddress);

    /**
     * @brief 启动注册中心服务器。
     * @note 调用 TcpServer::start，开始监听客户端连接。
     */
    void start();

    /**
     * @brief 设置心跳间隔时间
     * @param interval 心跳包间隔时间(单位:秒)
     */
    void setHeartbeatInterval(uint32_t interval) {
        if (!m_server.isStarted()) m_interval = interval;
    }

  private:
    /**
     * @brief 处理接收到的 RPC 消息。
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 解析后的 RpcMessage。
     * @note 根据消息类型分发到 handleRegister、handleDiscover 或 handleHeartbeat。
     * @note 不支持的消息类型记录警告日志。
     */
    void onRpcMessage(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 处理服务发现请求。
     *
     * 存储服务信息到 m_services，生成唯一 ID（格式：service_name@ip:port），返回 REGISTER_RESPONSE
     *
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 包含 DISCOVER_REQUEST 的 RpcMessage。
     * @note 查找 m_services 中的服务信息，返回 DISCOVER_RESPONSE，失败时设置状态码为 1。
     * @note 日志记录客户端地址和服务名称。
     */
    void handleRegister(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 处理服务发现请求。
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 包含 DISCOVER_REQUEST 的 RpcMessage。
     * @note 查找服务信息，返回 DISCOVER_RESPONSE，失败时设置非零状态码。
     */
    void handleDiscover(const TcpConnectionPtr& connection, const RpcMessage& message);

    /**
     * @brief 处理心跳请求。
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 包含 HEARTBEAT_REQUEST 的 RpcMessage。
     * @note 根据 service_id 更新 m_services 中对应服务的心跳时间，返回 HEARTBEAT_RESPONSE。
     * @note service_id 格式需为 ip:port，与注册时的 ID 匹配。
     */
    void handleHeartbeat(const TcpConnectionPtr& connection, const RpcMessage& message);

  private:
    TcpServer                 m_server;                       ///< TCP 服务器实例
    uint32_t                  m_interval;                     ///< 心跳包间隔时间,默认30s
    std::unique_ptr<RpcCodec> m_codec;                        ///< RPC 消息编解码器
    std::unordered_map<std::string, ServiceInfo> m_services;  ///< 服务信息映射，键为服务名
    ServiceHeartbeatQueue m_heartQueue;  ///< 维护一个心跳优先队列,将超时未接收到心跳包的服务移除
};
}  // namespace zmuduo::net::rpc

#endif