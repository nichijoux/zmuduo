#ifndef ZMUDUO_NET_RPC_RPC_CODEC_H
#define ZMUDUO_NET_RPC_RPC_CODEC_H

#include "zmuduo/net/buffer.h"
#include "zmuduo/net/rpc/rpc_core.h"
#include "zmuduo/net/tcp_connection.h"
#include <functional>

#include <utility>
namespace zmuduo::net::rpc {

/**
 * @class RpcCodec
 * @brief RPC 消息的编码和解码工具类。
 *
 * RpcCodec 负责将 RpcMessage（Protobuf 定义）编码为带长度头部的字节流，或从 TCP 连接的缓冲区解码为
 * RpcMessage。 支持异步消息处理，通过 MessageCallback 回调处理解析后的消息。
 *
 * @note 消息格式：4 字节长度头部（int32_t）+ 序列化的 RpcMessage 数据。
 * @note RpcCodec 线程安全，操作需在 EventLoop 线程中执行，MessageCallback 需用户确保线程安全。
 *
 * @example
 * @code
 * #include "zmuduo/net/rpc/rpc_codec.h"
 * zmuduo::net::rpc::RpcCodec codec([](const TcpConnectionPtr& conn, const RpcMessage& msg) {
 *     std::cout << "Received: " << msg.DebugString() << std::endl;
 * });
 * // 假设 conn 是已建立的 TcpConnection
 * RpcMessage msg;
 * msg.set_type(RPC_REQUEST);
 * RpcCodec::send(conn, msg);
 * @endcode
 */
class RpcCodec {
  public:
    /**
     * @typedef std::function&lt;void(const TcpConnectionPtr&, const RpcMessage&)&gt;
     * @brief RPC 消息处理回调类型。
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 解析后的 RpcMessage。
     */
    using MessageCallback =
        std::function<void(const TcpConnectionPtr& connection, const RpcMessage& message)>;

  public:
    /**
     * @brief 构造函数。
     * @param[in] cb 消息处理回调函数。
     * @note 回调函数在事件循环线程中执行，需确保线程安全。
     */
    explicit RpcCodec(MessageCallback cb) : callback(std::move(cb)) {}

    /**
     * @brief 处理接收到的 TCP 数据。
     * @param[in] connection TCP 连接智能指针。
     * @param[in,out] buffer 输入缓冲区，包含接收到的数据。
     * @param[in] receiveTime 消息接收时间戳。
     * @note 从缓冲区读取长度头部和消息数据，解析为 RpcMessage，触发回调。
     * @note 无效长度或解析失败将关闭连接。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

    /**
     * @brief 发送 RPC 消息。
     * @param[in] connection TCP 连接智能指针。
     * @param[in] message 要发送的 RpcMessage。
     * @note 消息序列化为字节流，添加 4 字节长度头部后发送。
     */
    static void send(const TcpConnectionPtr& connection, const RpcMessage& message);

  private:
    static constexpr size_t S_HEADER_LENGTH = sizeof(int32_t);  ///< 消息长度头部大小（4 字节）
    MessageCallback callback;                                   ///< 消息处理回调函数
};

}  // namespace zmuduo::net::rpc

#endif