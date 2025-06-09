#ifndef ZMUDUO_NET_RPC_RPC_CLIENT_H
#define ZMUDUO_NET_RPC_RPC_CLIENT_H

#include "zmuduo/base/mutex.h"
#include "zmuduo/net/rpc/rpc_channel.h"

namespace zmuduo::net::rpc {
/**
 * @class RpcClient
 * @brief RPC 客户端，负责服务发现和 RpcChannel 创建。
 *
 * RpcClient 通过服务缓存（m_serviceCache）或向注册中心（RegistryServer）发送 DISCOVER_REQUEST，<br/>
 * 获取服务地址并创建 RpcChannel。支持异步服务发现，使用读写锁保护缓存以确保线程安全。
 *
 * @note RpcClient 线程安全，操作需在 EventLoop 线程中执行，回调需用户确保线程安全。
 * @note 服务缓存未实现过期机制，可能导致使用失效地址。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/rpc_client.h"
 * #include <my_service.pb.h>
 * zmuduo::net::EventLoop loop;
 * auto registryAddr = zmuduo::net::IPv4Address::Create("127.0.0.1", 9999);
 * zmuduo::net::rpc::RpcClient client(&loop, registryAddr);
 * client.callService("MyService", [](const std::shared_ptr<RpcChannel>& channel) {
 *     if (channel) {
 *         MyService_Stub stub(channel.get());
 *         // 发起 RPC 调用
 *     }
 * });
 * loop.loop();
 * @endcode
 */
class RpcClient {
  public:
    /**
     * @typedef ChannelCallback
     * @brief RpcChannel 回调类型。
     * @param[in] channel 创建的 RpcChannel 智能指针，失败时为 nullptr。
     */
    using ChannelCallback = std::function<void(const std::shared_ptr<RpcChannel>&)>;

    /**
     * @typedef DiscoverCallback
     * @brief 服务发现回调类型。
     * @param[in] address 服务地址智能指针，失败时为 nullptr。
     */
    using DiscoverCallback = std::function<void(Address::Ptr)>;

    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] registryAddress 注册中心地址。
     * @note 初始化事件循环和注册中心地址。
     */
    RpcClient(EventLoop* loop, Address::Ptr registryAddress);

    /**
     * @brief 异步获取服务对应的 RpcChannel。
     * @param[in] serviceName 服务名称。
     * @param[in] callback 接收 RpcChannel 的回调。
     * @note 优先从缓存获取地址，未命中则触发服务发现，回调在 EventLoop 线程中执行。
     */
    void callService(const std::string& serviceName, const ChannelCallback& callback);

    /**
     * @brief 获取事件循环指针。
     * @return EventLoop* 事件循环指针。
     */
    EventLoop* getEventLoop() const {
        return m_eventLoop;
    }

  private:
    /**
     * @brief 异步发现服务地址。
     * @param[in] serviceName 服务名称。
     * @param[in] callback 接收服务地址的回调。
     * @note 向注册中心发送 DISCOVER_REQUEST，解析响应并回调地址。
     */
    void discoverService(const std::string& serviceName, const DiscoverCallback& callback);

  private:
    EventLoop*                                    m_eventLoop;        ///< 事件循环指针
    Address::Ptr                                  m_registryAddress;  ///< 注册中心地址
    ReadWriteLock                                 m_rwLock;  ///< 读写锁，保护服务缓存
    std::unordered_map<std::string, Address::Ptr> m_serviceCache;  ///< 服务名到地址的缓存
};
}  // namespace zmuduo::net::rpc

#endif