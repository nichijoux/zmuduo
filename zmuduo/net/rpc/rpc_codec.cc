#include "zmuduo/net/rpc/rpc_codec.h"
#include "zmuduo/base/logger.h"

namespace zmuduo::net::rpc {
void RpcCodec::send(const TcpConnectionPtr& connection, const rpc::RpcMessage& message) {
    Buffer buffer;
    // 要发生的消息
    std::string data = message.SerializeAsString();
    // 消息体长度
    buffer.prependInt32(static_cast<int32_t>(data.size()));
    buffer.write(data);
    // 发送消息
    connection->send(buffer);
}

void RpcCodec::onMessage(const TcpConnectionPtr& connection,
                         Buffer&                 buffer,
                         const Timestamp&        receiveTime) {
    while (buffer.getReadableBytes() >= S_HEADER_LENGTH) {
        // 先读取一个int32_t的长度
        const int32_t length = buffer.peekInt32();
        // 长度错误
        if (length < 0) {
            ZMUDUO_LOG_ERROR << "Invalid length " << length;
            connection->shutdown();
            break;
        } else if (buffer.getReadableBytes() >= length + S_HEADER_LENGTH) {
            // 如果剩余可读取字节数满足数据长度则进行读取
            buffer.retrieve(S_HEADER_LENGTH);
            // 读取数据
            std::string message = buffer.retrieveAsString(length);
            // 构造一个rpcMessage消息
            RpcMessage rpcMessage;
            // 解码消息并回调
            if (rpcMessage.ParseFromString(message)) {
                callback(connection, rpcMessage);
            } else {
                ZMUDUO_LOG_ERROR << "Parse error";
                connection->shutdown();
                break;
            }
        } else {
            break;
        }
    }
}
}  // namespace zmuduo::net::rpc