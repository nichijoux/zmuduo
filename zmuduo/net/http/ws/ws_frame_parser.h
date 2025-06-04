// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_HTTP_WS_WS_MESSAGE_PARSER_H
#define ZMUDUO_NET_HTTP_WS_WS_MESSAGE_PARSER_H

#include <utility>

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/http/ws/ws_frame.h"

namespace zmuduo::net::http::ws {
/**
 * @class WSFrameParser
 * @brief WebSocket 帧解析器。
 *
 * WSFrameParser 用于解析符合 RFC 6455 的 WebSocket 帧，支持客户端和服务器端帧的解析。<br/>
 * 通过状态机管理解析流程，处理帧头部、扩展长度、掩码密钥和 payload 数据。<br/>
 * 解析结果存储在 WSFrameMessage 中，错误信息通过 getErrorMessage 获取。
 *
 * @note WSFrameParser 不可拷贝（继承自 NoCopyable），通过 Buffer 管理输入数据。
 * @note 解析操作在 EventLoop 线程中执行，确保线程安全。
 *
 * @example
 *
 * @code
 * WSFrameParser parser;
 * Buffer buffer;
 * buffer.append(...); // 填充 WebSocket 帧数据
 * int result = parser.parse(buffer, true); // 解析客户端帧
 * if (result == 1) {
 *     const WSFrameMessage& msg = parser.getWSFrameMessage();
 *     std::cout << "Opcode: " << msg.m_opcode << ", Payload: " << msg.m_payload << std::endl;
 * } else if (result == -1) {
 *     std::cerr << "Error: " << parser.getErrorMessage() << std::endl;
 * }
 * @endcode
 */
class WSFrameParser : NoCopyable {
  public:
    /**
     * @typedef std::shared_ptr&lt;WSFrameParser&gt;
     * @brief ws帧解析器智能指针
     */
    using Ptr = std::shared_ptr<WSFrameParser>;

  public:
    /**
     * @brief 构造函数。
     * @note 初始化解析器状态为 NOT_START，清空错误信息和消息数据。
     */
    WSFrameParser()
        : m_state(State::NOT_START),
          m_error(),
          m_head(),
          m_payloadLength(0),
          m_maskKey(),
          m_message() {}

    /**
     * @brief 重置解析器。
     * @note 将解析状态、错误信息、帧头部、长度和消息数据恢复到初始状态。
     */
    void reset();

    /**
     * @brief 解析 WebSocket 帧（基于字符串）。
     * @param[in] string 输入的帧数据。
     * @param[in] isClient 是否为客户端发送的帧（影响掩码验证）。
     * @retval 0 待解析（数据不足）。
     * @retval 1 解析完成。
     * @retval -1 解析错误（可通过 getErrorMessage 获取错误信息）。
     * @note 将字符串转换为 Buffer 后调用 Buffer 版本的 parse。
     */
    int parse(const std::string& string, bool isClient) {
        Buffer buffer;
        buffer.write(string);
        return parse(buffer, isClient);
    }

    /**
     * @brief 解析 WebSocket 帧（基于 Buffer）。
     * @param[in,out] buffer 输入缓冲区，解析后更新可读位置。
     * @param[in] isClient 是否为客户端发送的帧（影响掩码验证）。
     * @retval 0 待解析（数据不足）。
     * @retval 1 解析完成。
     * @retval -1 解析错误（可通过 getErrorMessage 获取错误信息）。
     * @note 通过状态机解析帧头部、长度、掩码和 payload。
     */
    int parse(Buffer& buffer, bool isClient);

    /**
     * @brief 获取解析后的 WebSocket 帧消息。
     * @return 解析完成的 WSFrameMessage 对象。
     */
    const WSFrameMessage& getWSFrameMessage() const {
        return m_message;
    }

    /**
     * @brief 获取解析错误信息。
     * @return 错误信息字符串（若无错误则为空）。
     */
    const std::string& getErrorMessage() const {
        return m_error;
    }

  private:
    /**
     * @enum State
     * @brief 解析状态枚举。
     */
    enum class State {
        NOT_START,        ///< 未开始解析
        HEAD_PARSED,      ///< 帧头部解析完成
        LENGTH_PARSED,    ///< 扩展长度解析完成
        MASK_KEY_PARSED,  ///< 掩码密钥解析完成
        WAIT_OTHER,       ///< 等待后续分片数据
        FINISH,           ///< 解析完成
        ERROR             ///< 解析错误
    };

    /**
     * @brief 解析帧头部。
     * @param[in,out] buffer 输入缓冲区。
     * @param[in] isClient 是否为客户端发送的帧。
     * @note 解析 FIN、RSV、opcode 和初始 payload 长度，验证掩码和 opcode 合法性。
     */
    void parseHead(Buffer& buffer, bool isClient);

    /**
     * @brief 解析扩展长度。
     * @param[in,out] buffer 输入缓冲区。
     * @note 处理 payload 长度为 126 或 127 的情况，读取扩展长度字段。
     */
    void parseLength(Buffer& buffer);

    /**
     * @brief 解析掩码密钥。
     * @param[in,out] buffer 输入缓冲区。
     * @note 如果帧设置了掩码，则读取 4 字节掩码密钥。
     */
    void parseMaskKey(Buffer& buffer);

    /**
     * @brief 解析 payload 数据。
     * @param[in,out] buffer 输入缓冲区。
     * @note 读取 payload 数据并应用掩码解码，更新解析状态。
     */
    void parsePayload(Buffer& buffer);

    /**
     * @brief 设置解析错误。
     * @param[in] error 错误信息。
     * @note 将状态设置为 ERROR 并记录错误信息。
     */
    void setParseError(std::string error) {
        m_error = std::move(error);
        m_state = State::ERROR;
    }

  private:
    State m_state;               ///< 当前解析状态
    std::string m_error;         ///< 错误信息
    WSFrameHead m_head;          ///< 帧头部
    int64_t m_payloadLength;     ///< payload 长度
    char m_maskKey[4];           ///< 掩码密钥
    WSFrameMessage m_message;    ///< 解析后的消息
};
}  // namespace zmuduo::net::http::ws

#endif