#ifndef ZMUDUO_NET_RPC_RPC_CORE_H
#define ZMUDUO_NET_RPC_RPC_CORE_H

#include "zmuduo/base/logger.h"
#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/nomoveable.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/rpc/rpc.pb.h"
#include "zmuduo/net/rpc/rpc_client.h"
#include <functional>
#include <google/protobuf/service.h>
#include <memory>

namespace zmuduo::net::rpc {

/**
 * @class LambdaClosure
 * @brief Protobuf Closure 的轻量级实现，封装 std::function 回调。
 *
 * LambdaClosure 将 C++ lambda 或 std::function 封装为 Protobuf 的 Closure 接口，
 * 用于异步 RPC 调用的回调处理。调用 Run() 后自动销毁，防止内存泄漏。
 *
 * @note LambdaClosure 不可拷贝、不可移动，线程安全取决于封装的回调函数。
 * @note 使用 Create 静态方法创建实例，避免直接构造。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/rpc_core.h"
 * auto closure = LambdaClosure::Create([] { std::cout << "Callback executed" << std::endl; });
 * closure->Run(); // 自动销毁
 * @endcode
 */
class LambdaClosure : public google::protobuf::Closure {
  public:
    /**
     * @brief 执行回调并销毁自身。
     * @note 调用 m_function 后自动 delete this，防止内存泄漏。
     */
    void Run() override {
        m_function();
        delete this;
    }

    /**
     * @brief 静态工厂方法，创建 LambdaClosure 实例。
     * @tparam Function 回调函数类型（如 lambda 或 std::function）。
     * @param[in] func 回调函数。
     * @return google::protobuf::Closure 指针，需调用 Run() 后自动销毁。
     */
    template <typename Function>
    static google::protobuf::Closure* Create(Function&& func) {
        return new LambdaClosure([Func = std::forward<Function>(func)] { Func(); });
    }

  private:
    /**
     * @brief 构造函数。
     * @param[in] fn 封装的回调函数。
     * @note 仅由 Create 方法调用。
     */
    explicit LambdaClosure(std::function<void()> fn) : m_function(std::move(fn)) {}

    std::function<void()> m_function;  ///< 封装的回调函数
};

/**
 * @class RpcCaller
 * @brief 将 Protobuf 异步 RPC 调用封装为 C++ 回调的工具类。
 *
 * RpcCaller 提供静态方法 Call，将 Protobuf 的异步 RPC 调用（基于 Closure）封装为
 * C++ 回调形式，支持线程安全的异步操作。
 *
 * @note RpcCaller 不可拷贝、不可移动，所有方法为静态。
 * @note 回调函数在事件循环线程中执行，需确保线程安全。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/rpc_core.h"
 * #include <my_service.pb.h>
 * MyService_Stub stub(channel.get());
 * MyRequest request;
 * RpcCaller::Call(&stub, &MyService_Stub::MyMethod, request,
 *                 [](const MyResponse& response) {
 *                     std::cout << "Response: " << response.DebugString() << std::endl;
 *                 });
 * @endcode
 */
class RpcCaller : NoCopyable, NoMoveable {
  public:
    /**
     * @typedef RpcMethod
     * @brief RPC 方法类型模板。
     * @tparam Stub 服务 Stub 类型。
     * @tparam Request 请求消息类型。
     * @tparam Response 响应消息类型。
     */
    template <typename Stub, typename Request, typename Response>
    using RpcMethod = void (Stub::*)(::google::protobuf::RpcController*,
                                     const Request*,
                                     Response*,
                                     ::google::protobuf::Closure*);

    /**
     * @typedef DoneCallback
     * @brief RPC 响应回调类型模板。
     * @tparam Response 响应消息类型。
     * @param[in] response 响应消息。
     */
    template <typename Response>
    using DoneCallback = std::function<void(const Response&)>;

    /**
     * @brief 执行异步 RPC 调用。
     * @tparam Response 响应消息类型。
     * @tparam Stub 服务 Stub 类型。
     * @tparam Request 请求消息类型。
     * @param[in] stub 服务 Stub 指针。
     * @param[in] method RPC 方法。
     * @param[in] request 请求消息。
     * @param[in] callback 响应回调函数。
     * @note 使用 LambdaClosure 封装回调，自动管理内存。
     */
    template <typename Response, typename Stub, typename Request>
    static void Call(Stub*                              stub,
                     RpcMethod<Stub, Request, Response> method,
                     const Request&                     request,
                     DoneCallback<Response>             callback) {
        auto response    = std::make_shared<Response>();
        auto doneClosure = LambdaClosure::Create([response, done = std::move(callback)]() {
            if (done) { done(*response); }
        });

        (stub->*method)(nullptr, &request, response.get(), doneClosure);
    }
};

/**
 * @class RpcCallerClient
 * @brief RPC 客户端调用工具类，支持异步服务调用。
 *
 * RpcCallerClient 通过 RpcClient 与注册中心通信，获取服务通道（RpcChannel），
 * 并执行异步 RPC 调用。支持服务发现和回调处理。
 *
 * @note RpcCallerClient 不可拷贝，线程安全，操作需在 EventLoop 线程中执行。
 * @note ResponseCallback 和 NotFoundChannelCallback 需用户确保线程安全。
 *
 * @example
 *
 * @code
 * #include "zmuduo/net/rpc/rpc_core.h"
 * #include <my_service.pb.h>
 * zmuduo::net::EventLoop loop;
 * auto addr = zmuduo::net::IPv4Address::Create("127.0.0.1", 8888);
 * RpcCallerClient<MyService_Stub> client(&loop, addr);
 * MyRequest request;
 * client.callServiceMethod<MyResponse, MyRequest>(
 *     "MyService", &MyService_Stub::MyMethod, request,
 *     [](const MyResponse& response) {
 *         std::cout << "Response: " << response.DebugString() << std::endl;
 *     },
 *     [] { std::cout << "Service not found" << std::endl; });
 * loop.loop();
 * @endcode
 */
template <typename Stub>
class RpcCallerClient : NoCopyable {
  public:
    /**
     * @typedef RpcMethod
     * @brief RPC 方法类型别名。
     * @tparam Request 请求消息类型。
     * @tparam Response 响应消息类型。
     */
    template <typename Request, typename Response>
    using RpcMethod = void (Stub::*)(::google::protobuf::RpcController*,
                                     const Request*,
                                     Response*,
                                     ::google::protobuf::Closure*);

    /**
     * @typedef ResponseCallback
     * @brief 响应回调类型。
     * @tparam Response 响应消息类型。
     * @param[in] response 响应消息。
     */
    template <typename Response>
    using ResponseCallback = std::function<void(const Response&)>;

    /**
     * @typedef NotFoundChannelCallback
     * @brief 服务未找到时的回调类型。
     */
    using NotFoundChannelCallback = std::function<void()>;

    /**
     * @brief 构造函数。
     * @param[in] loop 事件循环指针。
     * @param[in] registryAddress 注册中心地址。
     * @note 初始化 RpcClient，用于与注册中心通信。
     */
    explicit RpcCallerClient(EventLoop* loop, const Address::Ptr& registryAddress)
        : m_client(loop, registryAddress) {}

  protected:
    /**
     * @brief 调用远程服务方法。
     * @tparam Response 响应消息类型。
     * @tparam Request 请求消息类型。
     * @param[in] serviceName 服务名称。
     * @param[in] method RPC 方法。
     * @param[in] request 请求消息。
     * @param[in] callback 响应回调函数。
     * @param[in] notFoundCallback 服务未找到时的回调（可选）。
     * @note 通过 runInLoop 确保线程安全，异步获取 RpcChannel 并调用服务。
     */
    template <typename Response, typename Request>
    void callServiceMethod(const std::string&           serviceName,
                           RpcMethod<Request, Response> method,
                           Request                      request,
                           ResponseCallback<Response>   callback,
                           NotFoundChannelCallback      notFoundCallback = nullptr) {
        m_client.getEventLoop()->runInLoop(
            [serviceName, method = std::move(method), request = std::move(request),
             callback = std::move(callback), notFoundCallback = std::move(notFoundCallback),
             this]() mutable {
                m_client.callService(
                    serviceName, [serviceName, method = std::move(method),
                                  request = std::move(request), callback = std::move(callback),
                                  notFoundCallback = std::move(notFoundCallback)](
                                     const std::shared_ptr<RpcChannel>& channel) mutable {
                        if (!channel) {
                            if (notFoundCallback) {
                                notFoundCallback();
                            } else {
                                ZMUDUO_LOG_ERROR << "Failed to get channel for service: "
                                                 << serviceName;
                            }
                            return;
                        }
                        // 创建Stub并调用
                        Stub stub(channel.get());
                        // 使用RpcCaller进行调用
                        RpcCaller::Call(&stub, method, request, std::move(callback));
                    });
            });
    }

  private:
    RpcClient m_client;  ///< RPC 客户端，负责与注册中心通信
};

}  // namespace zmuduo::net::rpc

#endif