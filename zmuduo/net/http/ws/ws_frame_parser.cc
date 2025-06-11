#include "zmuduo/net/http/ws/ws_frame_parser.h"

namespace zmuduo::net::http::ws {
void WSFrameParser::reset() {
    m_state = State::NOT_START;
    m_error.clear();
    // head其实可以不清理,因为解析一定会重新赋值的
    bzero(&m_head, sizeof(m_head));
    m_payloadLength    = 0;
    m_message.m_opcode = 0;
    m_message.m_payload.clear();
}

int WSFrameParser::parse(Buffer& buffer, bool isClient) {
    State beforeState;
    do {
        beforeState = m_state;
        switch (m_state) {
            case State::NOT_START: parseHead(buffer, isClient); break;
            case State::HEAD_PARSED: parseLength(buffer); break;
            case State::LENGTH_PARSED: parseMaskKey(buffer); break;
            case State::MASK_KEY_PARSED: parsePayload(buffer); break;
            case State::WAIT_OTHER:
                // 重置部分数据,重置为not_start会再次解析数据
                m_state         = State::NOT_START;
                m_payloadLength = 0;
                break;
            case State::FINISH:
            case State::ERROR: break;
        }
    } while (beforeState != m_state);
    int code = m_state == State::FINISH ? 1 : m_error.empty() ? 0 : -1;
    return code;
}

void WSFrameParser::parseHead(Buffer& buffer, bool isClient) {
    if (buffer.getReadableBytes() >= sizeof(m_head)) {
        // 第一个字节：FIN, RSV1-3, Opcode
        m_head.fin    = (*buffer.peek() & 0x80) != 0;
        m_head.rsv1   = (*buffer.peek() & 0x40) != 0;
        m_head.rsv2   = (*buffer.peek() & 0x20) != 0;
        m_head.rsv3   = (*buffer.peek() & 0x10) != 0;
        m_head.opcode = *buffer.peek() & 0x0F;
        buffer.retrieve(1);
        // 第二个字节：MASK, Payload Length
        m_head.mask          = (*buffer.peek() & 0x80) != 0;
        m_head.payloadLength = *buffer.peek() & 0x7F;
        buffer.retrieve(1);
        // 解析成功后判断是否存在错误
        if (isClient && m_head.mask != 1) {
            setParseError("the client websocket frame head mask must be 1");
        } else if (!isClient && m_head.mask == 1) {
            // 如果是服务器端发送的
            setParseError("the server websocket frame head mask must be 0");
        } else if (!m_head.isValidOpcode()) {
            setParseError("websocket opcode is invalid");
        } else if ((m_head.opcode & 0b1000) != 0 && !m_head.fin) {
            // 如果是控制帧且分片了,则说明出现异常
            setParseError("control frame can't be divide");
        } else {
            m_message.m_opcode = m_head.opcode;
            m_state            = State::HEAD_PARSED;
        }
    }
}

void WSFrameParser::parseLength(Buffer& buffer) {
    // 进到该函数则说明opcode一定合法
    // 解析扩展长度
    if (m_head.isControlFrame()) {
        // 控制帧
        if (m_head.payloadLength >= 126) {
            setParseError("控制帧（如 PING） 数据长度不能超过125");
        } else {
            m_payloadLength = m_head.payloadLength;
            m_state         = State::LENGTH_PARSED;
        }
    } else {
        // 数据帧
        if (m_head.payloadLength == 126 && buffer.getReadableBytes() >= 2) {
            // 如果是126,则接下来2个字节长度为数据长度
            m_payloadLength = buffer.readInt16();
            m_state         = State::LENGTH_PARSED;
        } else if (m_head.payloadLength == 127 && buffer.getReadableBytes() >= 8) {
            // 如果是127,则接下来8个字节长度为数据产嘀咕
            m_payloadLength = buffer.readInt64();
            m_state         = State::LENGTH_PARSED;
        } else if (m_head.payloadLength < 126) {
            m_payloadLength = m_head.payloadLength;
            m_state         = State::LENGTH_PARSED;
        }
    }
}

void WSFrameParser::parseMaskKey(Buffer& buffer) {
    if (m_head.mask) {
        // 如果需要掩码
        if (buffer.getReadableBytes() >= sizeof(m_maskKey)) {
            // 掩码
            memcpy(m_maskKey, buffer.peek(), sizeof(m_maskKey));
            buffer.retrieve(sizeof(m_maskKey));
            m_state = State::MASK_KEY_PARSED;
        }
    } else {
        // 不需要掩码,直接宣告结束
        m_state = State::MASK_KEY_PARSED;
    }
}

void WSFrameParser::parsePayload(Buffer& buffer) {
    size_t beforeIndex = m_message.m_payload.size();
    // 不断读取buffer中的数据
    while (buffer.getReadableBytes() >= m_payloadLength && m_payloadLength != 0) {
        // 读取长度不能超过m_payloadLength
        auto length = std::min(static_cast<int64_t>(buffer.getReadableBytes()), m_payloadLength);
        // 先将buffer的数据读取到data中
        m_message.m_payload.append(buffer.peek(), length);
        buffer.retrieve(length);
        m_payloadLength -= length;
        // 使用mask解密数据
        if (m_head.mask) {
            for (int i = 0; i < length; i++) {
                m_message.m_payload[beforeIndex + i] ^= m_maskKey[i % sizeof(m_maskKey)];
            }
        }
        beforeIndex += m_payloadLength;
    }
    // 如果数据接收完毕,则说明结束
    if (m_payloadLength == 0) {
        m_state = m_head.fin ? State::FINISH : State::WAIT_OTHER;
    }
}
}  // namespace zmuduo::net::http::ws