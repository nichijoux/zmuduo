// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_WS_WS_MESSAGE_H
#define ZMUDUO_NET_HTTP_WS_WS_MESSAGE_H

#include "zmuduo/base/copyable.h"
#include "zmuduo/net/buffer.h"
#include "zmuduo/net/callbacks.h"
#include <any>
#include <memory>
#include <string>

namespace zmuduo::net::http::ws {

/**
 * @brief WebSocket 关闭代码枚举。
 *
 * 定义符合 RFC 6455 标准的 WebSocket 关闭代码，用于 CLOSE 帧的状态码。
 * 包括标准代码（1000-1015）、库/框架代码（3000-3999）和私有代码（4000-4999）。
 */
#define WS_CLOSE_CODE_MAP(XX)                                                                      \
    XX(1000, NORMAL_CLOSURE, "Normal closure")                                                     \
    XX(1001, GOING_AWAY, "Going away")                                                             \
    XX(1002, PROTOCOL_ERROR, "Protocol error")                                                     \
    XX(1003, UNSUPPORTED_DATA, "Unsupported data")                                                 \
    XX(1004, RESERVED, "Reserved")                                                                 \
    XX(1005, NO_STATUS_RECEIVED, "No status received")                                             \
    XX(1006, ABNORMAL_CLOSURE, "Abnormal closure")                                                 \
    XX(1007, INVALID_PAYLOAD_DATA, "Invalid frame payload data")                                   \
    XX(1008, POLICY_VIOLATION, "Policy violation")                                                 \
    XX(1009, MESSAGE_TOO_BIG, "Message too big")                                                   \
    XX(1010, MANDATORY_EXTENSION, "Mandatory extension")                                           \
    XX(1011, INTERNAL_ERROR, "Internal server error")                                              \
    XX(1012, SERVICE_RESTART, "Service restart")                                                   \
    XX(1013, TRY_AGAIN_LATER, "Try again later")                                                   \
    XX(1014, BAD_GATEWAY, "Bad gateway")                                                           \
    XX(1015, TLS_HANDSHAKE, "TLS handshake failure")                                               \
    XX(3000, UNAUTHORIZED, "Unauthorized")                                                         \
    XX(3001, FORBIDDEN, "Forbidden")                                                               \
    XX(3002, NOT_FOUND, "Not found")                                                               \
    XX(3003, METHOD_NOT_ALLOWED, "Method not allowed")                                             \
    XX(3007, REQUIRED, "Required")                                                                 \
    XX(3008, CONFLICT, "Conflict")                                                                 \
    XX(3009, GONE, "Gone")                                                                         \
    XX(3010, LENGTH_REQUIRED, "Length required")                                                   \
    XX(3011, PRECONDITION_FAILED, "Precondition failed")                                           \
    XX(3012, ENTITY_TOO_LARGE, "Entity too large")                                                 \
    XX(3013, UNSUPPORTED_MEDIA_TYPE, "Unsupported media type")                                     \
    XX(3014, RANGE_NOT_SATISFIABLE, "Range not satisfiable")                                       \
    XX(3015, EXPECTATION_FAILED, "Expectation failed")                                             \
    XX(3016, MISDIRECTED_REQUEST, "Misdirected request")                                           \
    XX(3017, UPGRADE_REQUIRED, "Upgrade required")                                                 \
    XX(3018, PRECONDITION_REQUIRED, "Precondition required")                                       \
    XX(3019, TOO_MANY_REQUESTS, "Too many requests")                                               \
    XX(3020, REQUEST_HEADER_FIELDS_TOO_LARGE, "Request header fields too large")                   \
    XX(3021, UNAVAILABLE_FOR_LEGAL_REASONS, "Unavailable for legal reasons")

enum class WSCloseCode {
#define XX(num, name, string) name = num,
    WS_CLOSE_CODE_MAP(XX)
#undef XX
        INVALID_CLOSE_CODE  ///< 无效关闭代码
};

/**
 * @brief 将字符串转换为 WSCloseCode 枚举。
 * @param[in] s 关闭代码的字符串表示（如 "Normal closure"）。
 * @return 对应的 WSCloseCode 枚举值，若无效则返回 INVALID_CLOSE_CODE。
 */
WSCloseCode CharsToWSCloseCode(const char* s);

#pragma pack(1)
/**
 * @brief WebSocket 帧头部结构体。
 *
 * 定义符合 RFC 6455 的 WebSocket 帧头部结构，包含 FIN、RSV、opcode、mask 和 payload 长度字段。
 * 用于描述 WebSocket 数据帧和控制帧的元数据。
 *
 * @note 结构体使用 `#pragma pack(1)` 确保紧凑布局，避免字节对齐问题。
 * @note 帧格式如下：
 *
 * @code
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0
 * +-+-+-+-+-------+-+-------------+----------------------------+
 * |F|R|R|R| opcode|M| Payload len |    Extended payload length |
 * |I|S|S|S|  (4)  |A|     (7)     |             (16/64)        |
 * |N|V|V|V|       |S|             |   (if payload len==126/127)|
 * | |1|2|3|       |K|             |                            |
 * +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - -+
 * |   Extended payload length continued, if payload len == 127 |
 * + - - - - - - - - - - - - - - - +----------------------------+
 * |                               |Masking-key,if MASK set to 1|
 * +-------------------------------+----------------------------+
 * | Masking-key (continued)       |          Payload Data      |
 * +-------------------------------- - - - - - - - - - - - - - -+
 * :                     Payload Data continued ...             :
 * + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -+
 * |                     Payload Data continued ...             |
 * +------------------------------------------------------------+
 * @endcode
 *
 * @example
 *
 * @code
 * WSFrameHead head;
 * head.fin = true;
 * head.opcode = WSFrameHead::TEXT_FRAME;
 * head.mask = true;
 * head.payloadLength = 125;
 * std::cout << head.toString() << std::endl;
 * @endcode
 */
struct WSFrameHead {
    enum {
        CONTINUE   = 0,    ///< 数据分片帧
        TEXT_FRAME = 1,    ///< 文本帧
        BIN_FRAME  = 2,    ///< 二进制帧
        CLOSE      = 8,    ///< 关闭帧
        PING       = 0x9,  ///< PING 帧
        PONG       = 0xA   ///< PONG 帧
    };

    /**
     * @brief 判断当前帧是否为控制帧。
     * @retval true 当前帧是控制帧（CLOSE, PING, PONG）。
     * @retval false 当前帧不是控制帧（TEXT, BINARY, CONTINUATION）。
     */
    bool isControlFrame() const {
        return !isDataFrame();
    }

    /**
     * @brief 判断当前帧是否为数据帧。
     * @retval true 当前帧是数据帧（TEXT, BINARY, CONTINUATION）。
     * @retval false 当前帧不是数据帧（CLOSE, PING, PONG）。
     */
    bool isDataFrame() const {
        return (opcode & 0b1000) == 0;
    }

    /**
     * @brief 判断 opcode 是否合法。
     * @retval true opcode 是有效值（CONTINUE, TEXT_FRAME, BIN_FRAME, CLOSE, PING, PONG）。
     * @retval false opcode 是无效或预留值。
     */
    bool isValidOpcode() const {
        return opcode == CONTINUE || opcode == TEXT_FRAME || opcode == BIN_FRAME ||
               opcode == CLOSE || opcode == PING || opcode == PONG;
    }

    /**
     * @brief 将帧头部转换为字符串表示。
     * @return 包含 FIN、RSV、opcode、mask 和 payloadLength 的字符串描述。
     */
    std::string toString() const;

  public:
    bool     fin : 1;            ///< 是否为消息的最后一帧（true 表示最后一帧）
    bool     rsv1 : 1;           ///< 保留位 1，通常为 0，除非协商了扩展
    bool     rsv2 : 1;           ///< 保留位 2，通常为 0，除非协商了扩展
    bool     rsv3 : 1;           ///< 保留位 3，通常为 0，除非协商了扩展
    uint8_t  opcode : 4;         ///< 帧类型（0x0-0xF，定义帧用途）
    bool     mask : 1;           ///< 是否使用掩码（客户端发送必须为 true）
    uint16_t payloadLength : 7;  ///< payload 长度（0-125 或 126/127 表示扩展长度）
};
#pragma pack()

/**
 * @brief WebSocket 子协议接口。
 *
 * 定义 WebSocket 子协议的抽象基类，用于处理特定格式的 payload 数据（如 JSON、Protobuf）。<br/>
 * 子类需实现协议名称和数据处理逻辑。
 *
 * @example
 *
 * @code
 * class JsonSubProtocol : public WSSubProtocol {
 * public:
 *     std::string getName() const override { return "json"; }
 *     std::string process(const std::string& payload) override {
 *         // 假设 payload 是 JSON 字符串
 *         return "{\"echo\":\"" + payload + "\"}";
 *     }
 * };
 * @endcode
 */
class WSSubProtocol {
  public:
    using Ptr = std::shared_ptr<WSSubProtocol>;  ///< 智能指针类型

    virtual ~WSSubProtocol() = default;

    /**
     * @brief 获取子协议名称。
     * @return 子协议名称（如 "json", "protobuf"）。
     */
    virtual std::string getName() const = 0;

    /**
     * @brief 处理 payload 数据。
     * @param[in] payload 输入数据。
     * @return 处理后的数据。
     */
    virtual std::any process(const std::string& payload) = 0;

  private:
    std::string m_name;  ///< 协议名称
};

/**
 * @brief websocket子协议比较器
 *
 * @note 通过比较子协议的名称进行比较
 * @note 忽略大小写
 */
struct WSSubProtocolCompare {
    bool operator()(const WSSubProtocol::Ptr& a, const WSSubProtocol::Ptr& b) const {
        // 控制帧
        if (!a) {
            return b != nullptr;
        }
        if (!b) {
            return false;
        }
        // 忽略大小写
        return strcasecmp(a->getName().c_str(), b->getName().c_str()) < 0;
    }
};

/**
 * @brief WebSocket 数据帧。
 *
 * 表示 WebSocket 协议中的数据帧或控制帧，包含 opcode、payload 和可选的子协议。<br/>
 * 支持序列化为符合 RFC 6455 的二进制格式。
 *
 * @note WSFrameMessage 可拷贝（继承自 Copyable），通过智能指针（Ptr）管理。
 * @note 客户端发送的帧必须设置 mask，服务器发送的帧不设置 mask。
 *
 * @example
 *
 * @code
 * WSFrameMessage frame(WSFrameHead::TEXT_FRAME, "Hello, WebSocket!");
 * std::string data = frame.serialize(true); // 客户端序列化
 * // 通过 TcpConnection 发送 data
 * @endcode
 */
struct WSFrameMessage : public Copyable {
  public:
    using Ptr = std::shared_ptr<WSFrameMessage>;  ///< 智能指针类型

    /**
     * @brief 构造 CLOSE 帧。
     * @param[in] statusCode 关闭代码（如 WSCloseCode::NORMAL_CLOSURE）。
     * @param[in] reason 关闭原因（UTF-8 编码，可选）。
     * @return 构造好的 CLOSE 帧。
     */
    static WSFrameMessage MakeCloseFrame(WSCloseCode statusCode, const std::string& reason);

  public:
    /**
     * @brief 构造函数。
     * @param[in] opcode 帧类型（如 WSFrameHead::TEXT_FRAME）。
     * @param[in] data payload 数据（默认空）。
     * @param[in] subProtocol 子协议（默认 nullptr）。
     */
    explicit WSFrameMessage(uint8_t            opcode      = 0,
                            std::string        data        = "",
                            WSSubProtocol::Ptr subProtocol = nullptr)
        : m_opcode(opcode), m_payload(std::move(data)), m_subProtocol(std::move(subProtocol)) {}

    /**
     * @brief 判断当前帧是否为控制帧。
     * @retval true 当前帧是控制帧（CLOSE, PING, PONG）。
     * @retval false 当前帧不是控制帧（TEXT, BINARY, CONTINUATION）。
     */
    bool isControlFrame() const {
        return !isDataFrame();
    }

    /**
     * @brief 判断当前帧是否为数据帧。
     * @retval true 当前帧是数据帧（TEXT, BINARY, CONTINUATION）。
     * @retval false 当前帧不是控制帧（CLOSE, PING, PONG）。
     */
    bool isDataFrame() const {
        return (m_opcode & 0b1000) == 0;
    }

    /**
     * @brief 序列化为二进制数据。
     * @param[in] isClient 是否为客户端发送（true 表示需要掩码）。
     * @return 符合 RFC 6455 的二进制数据。
     * @note 客户端发送的帧必须设置 mask，服务器发送的帧不设置 mask。
     */
    std::string serialize(bool isClient) const;

  public:
    uint8_t            m_opcode;       ///< 帧类型
    std::string        m_payload;      ///< 数据内容
    WSSubProtocol::Ptr m_subProtocol;  ///< 子协议
};

/**
 * @brief 处理 WebSocket 控制帧。
 * @param[in] connection TCP 连接，表示 WebSocket 会话。
 * @param[in] message 控制帧消息（PING, PONG, CLOSE）。
 * @param[in] isFromClient 是否来自客户端（决定掩码设置）。
 * @note 分发到具体的控制帧处理函数（如 handleWSFramePing）。
 * @throw std::runtime_error 如果 message 不是控制帧（通过 assert 触发）。
 */
void handleWSFrameControl(const TcpConnectionPtr& connection,
                          const WSFrameMessage&   message,
                          bool                    isFromClient);

}  // namespace zmuduo::net::http::ws

#endif