#include "zmuduo/net/http/ws/ws_frame.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/net/endian.h"
#include "zmuduo/net/tcp_connection.h"

using namespace zmuduo::utils::hash_util;

namespace zmuduo::net::http::ws {
WSCloseCode CharsToWSCloseCode(const char* s) {
#define XX(num, name, string)                                                                      \
    if (strncmp(s, #string, strlen(#string)) == 0) {                                               \
        return WSCloseCode::name;                                                                  \
    }
    WS_CLOSE_CODE_MAP(XX)
#undef XX
    return WSCloseCode::INVALID_CLOSE_CODE;
}

/**
 * @brief 响应 WebSocket 的 PING 帧，发送对应的 PONG 帧。
 *
 * 当接收到一个 PING 帧时，本函数构造一个带有相同 payload 的 PONG 帧并发送回去，
 * 以符合 WebSocket 协议的心跳机制要求。
 *
 * @param[in] connection 当前的 TCP 连接（表示 WebSocket 会话）
 * @param[in] message 接收到的 PING 控制帧消息（其 opcode 应为 WSFrameHead::PING）
 * @param[in] isFromClient 标记当前是否为客户端发送过来的数据。该参数影响帧的 mask 标记：<br/>
 *                     - 客户端发送的帧需设置 mask；<br/>
 *                     - 服务器发送的帧不设置 mask。
 *
 * @note 该函数假设接收到的 message 一定是 PING 帧，并通过 assert 强制检查。
 *       若你希望支持 PONG 或其他控制帧，请使用更通用版本。
 *
 * @throws 程序断言失败（assert）如果 message 的 opcode 不是 WSFrameHead::PING。
 */
void handleWSFramePing(const TcpConnectionPtr& connection,
                       const WSFrameMessage&   message,
                       bool                    isFromClient) {
    assert(message.m_opcode == WSFrameHead::PING);
    connection->send(WSFrameMessage(WSFrameHead::PONG, message.m_payload).serialize(!isFromClient));
}

/**
 * @brief PONG帧的处理函数
 *
 * 用于处理接收到的 WebSocket PONG 控制帧。
 * 通常用于响应PING帧，无需特别操作，但可以在此处添加日志或心跳逻辑。
 */
void handleWSFramePong() {
    // 目前为空实现，可根据需要添加处理逻辑（例如更新心跳状态）
}

/**
 * @brief 处理接收到的 WebSocket CLOSE 控制帧
 *
 * 根据 WebSocket 协议，当收到一个CLOSE帧时，需要立即回复一个CLOSE帧（若尚未发送），
 * 并关闭底层TCP连接。
 *
 * @param[in] connection 当前连接
 * @param[in] message 接收到的CLOSE帧，包含状态码和可选关闭原因
 * @param[in] isFromClient 表示消息是否来自客户端（用于决定发送帧是否需要掩码）
 *
 * @throws 程序断言失败（assert）如果 message 的 opcode 不是 WSFrameHead::CLOSE。
 */
void handleWSFrameClose(const TcpConnectionPtr& connection,
                        const WSFrameMessage&   message,
                        bool                    isFromClient) {
    assert(message.m_opcode == WSFrameHead::CLOSE);
    // 提取前两个字节作为状态码（网络字节序）
    uint16_t statusCode;
    memcpy(&statusCode, message.m_payload.data(), 2);
    // 输出关闭原因（从第3个字节开始为UTF-8编码的关闭说明）
    ZMUDUO_LOG_INFO << message.m_payload.data() + 2;
    // 回复一个CLOSE帧（带回原始状态码与原因），掩码方向取决于消息来源
    connection->send(
        WSFrameMessage(WSFrameHead::CLOSE, message.m_payload).serialize(!isFromClient));
    // 强制关闭底层TCP连接，释放资源，并且不再接收消息
    connection->forceClose();
}

void handleWSFrameControl(const TcpConnectionPtr& connection,
                          const WSFrameMessage&   message,
                          bool                    isFromClient) {
    // 确保处理的是控制帧而不是数据帧
    assert(message.isControlFrame());
    // 根据帧类型分发处理逻辑
    switch (message.m_opcode) {
        case WSFrameHead::PING: handleWSFramePing(connection, message, isFromClient); break;
        case WSFrameHead::PONG: handleWSFramePong(); break;
        case WSFrameHead::CLOSE: handleWSFrameClose(connection, message, isFromClient); break;
    }
}

std::string WSFrameHead::toString() const {
    std::stringstream ss;
    ss << "[WSFrameHead fin = " << fin << " rsv1 = " << rsv1 << " rsv2 = " << rsv2
       << " rsv3 = " << rsv3 << " opcode = " << opcode << " mask = " << mask
       << " payload = " << payloadLength << "]";
    return ss.str();
}

WSFrameMessage WSFrameMessage::MakeCloseFrame(WSCloseCode statusCode, const std::string& reason) {
    std::string data;
    // 预留内存，避免拷贝
    data.reserve(2 + reason.size());
    // 状态码
    data.push_back(static_cast<char>((static_cast<uint16_t>(statusCode) >> 8) & 0xFF));
    data.push_back(static_cast<char>((static_cast<uint16_t>(statusCode) & 0xFF)));
    // 原因
    data.append(reason);
    return WSFrameMessage(WSFrameHead::CLOSE, std::move(data));
}

std::string WSFrameMessage::serialize(bool isClient) const {
    Buffer buffer;
    // 初始化消息头
    WSFrameHead head{};
    bzero(&head, sizeof(head));
    head.fin    = true;
    head.opcode = m_opcode;
    head.mask   = isClient;
    // 长度设置
    size_t size = m_payload.size();
    if (size < 126) {
        head.payloadLength = size;
    } else if (size < 65536) {
        head.payloadLength = 126;
    } else {
        head.payloadLength = 127;
    }
    // 写入头部
    buffer.writeInt8(static_cast<int8_t>(
        static_cast<int8_t>(head.fin << 7) | static_cast<int8_t>(head.rsv1 << 6) |
        static_cast<int8_t>(head.rsv2 << 5) | static_cast<int8_t>(head.rsv3 << 4) |
        static_cast<int8_t>(head.opcode & 0x0F)));
    buffer.writeInt8(static_cast<int8_t>(static_cast<int8_t>(head.mask << 7) |
                                         static_cast<int8_t>(head.payloadLength & 0x7F)));
    // 写入扩展长度
    if (head.payloadLength == 126) {
        uint16_t len = size;
        len          = byteSwapOnLittleEndian(len);
        buffer.write(&len, sizeof(len));
    } else if (head.payloadLength == 127) {
        uint64_t len = byteSwapOnLittleEndian(size);
        buffer.write(&len, sizeof(len));
    }
    auto data = m_payload;
    // 写入mask
    if (head.mask) {
        auto maskKey = RandomString(4);
        buffer.write(maskKey);
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= maskKey[i % maskKey.size()];
        }
    }
    // 写入数据
    buffer.write(data);
    return buffer.retrieveAllAsString();
}
}  // namespace zmuduo::net::http::ws