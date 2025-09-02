#include "zmuduo/net/buffer.h"
#include <algorithm>
#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>

namespace zmuduo::net {
void Buffer::retrieve(const size_t length) {
    assert(length <= getReadableBytes());
    if (length < getReadableBytes()) {
        // 读取length长度的内容
        m_readerIndex += length;
    } else {
        // m_readerIndex == getReadableBytes()
        // 读取缓冲区所有可读的内容
        retrieveAll();
    }
}

std::string Buffer::retrieveAsString(const size_t length) {
    assert(length <= getReadableBytes());
    std::string result(peek(), length);
    retrieve(length);
    return result;
}

#define PEEK_DATA(type)                                                                            \
    do {                                                                                           \
        assert(getReadableBytes() >= sizeof(type));                                                \
        type data;                                                                                 \
        memcpy(&data, peek(), sizeof(data));                                                       \
        return static_cast<type>(byteSwapOnLittleEndian(data));                                    \
    } while (false)

int64_t Buffer::peekInt64() const {
    PEEK_DATA(int64_t);
}

int32_t Buffer::peekInt32() const {
    PEEK_DATA(int32_t);
}

int16_t Buffer::peekInt16() const {
    PEEK_DATA(int64_t);
}

int8_t Buffer::peekInt8() const {
    PEEK_DATA(int64_t);
}

#undef PEEK_DATA

void Buffer::prepend(const void* data, const size_t length) {
    assert(length <= getPrependableBytes());
    m_readerIndex -= length;
    const auto ptr = static_cast<const char*>(data);
    std::copy_n(ptr, length, const_cast<char*>(begin()) + m_readerIndex);
}

#define PREPEND_DATA(x)                                                                            \
    do {                                                                                           \
        auto data = byteSwapOnLittleEndian(x);                                                     \
        prepend(&data, sizeof(data));                                                              \
    } while (false)

void Buffer::prependInt64(const int64_t x) {
    PREPEND_DATA(x);
}

void Buffer::prependInt32(const int32_t x) {
    PREPEND_DATA(x);
}

void Buffer::prependInt16(const int16_t x) {
    PREPEND_DATA(x);
}

void Buffer::prependInt8(const int8_t x) {
    PREPEND_DATA(x);
}

#undef PREPEND_DATA

void Buffer::retrieveInt64() {
    retrieve(sizeof(int64_t));
}

void Buffer::retrieveInt32() {
    retrieve(sizeof(int32_t));
}

void Buffer::retrieveInt16() {
    retrieve(sizeof(int16_t));
}

void Buffer::retrieveInt8() {
    retrieve(sizeof(int8_t));
}

void Buffer::write(const void* data, const size_t length) {
    ensureWritableBytes(length);
    memcpy(const_cast<char*>(getBeginWrite()), data, length);
    hasWritten(length);
}

#define WRITE_DATA(x)                                                                              \
    do {                                                                                           \
        decltype(x) data = byteSwapOnLittleEndian(x);                                              \
        write(reinterpret_cast<const char*>(&data), sizeof(data));                                 \
    } while (false)

void Buffer::writeInt64(int64_t x) {
    WRITE_DATA(x);
}

void Buffer::writeInt32(int32_t x) {
    WRITE_DATA(x);
}

void Buffer::writeInt16(int16_t x) {
    WRITE_DATA(x);
}

void Buffer::writeInt8(int8_t x) {
    WRITE_DATA(x);
}

#undef WRITE_DATA

void Buffer::read(void* dist, const size_t length) {
    assert(getReadableBytes() >= length);
    memcpy(dist, peek(), length);
    retrieve(length);
}

#define READ_DATA(type)                                                                            \
    do {                                                                                           \
        assert(getReadableBytes() >= sizeof(type));                                                \
        type data;                                                                                 \
        memcpy(&data, peek(), sizeof(data));                                                       \
        data = static_cast<type>(byteSwapOnLittleEndian(data));                                    \
        retrieve(sizeof(type));                                                                    \
        return data;                                                                               \
    } while (false)

int64_t Buffer::readInt64() {
    READ_DATA(int64_t);
}

int32_t Buffer::readInt32() {
    READ_DATA(int32_t);
}

int16_t Buffer::readInt16() {
    READ_DATA(int16_t);
}

int8_t Buffer::readInt8() {
    READ_DATA(int8_t);
}
#undef READ_DATA

void Buffer::unwrite(const size_t length) {
    assert(length <= getReadableBytes());
    m_readerIndex -= length;
}

ssize_t Buffer::readFD(const int fd, int* savedErrno) {
    char extraBuffer[65536] = {};
    // readv,writev
    iovec vec[2];
    // buffer缓冲区中剩余的可写空间大小
    const size_t writeable = getWriteableBytes();
    // 将vec[0]指向buffer缓冲区中剩余的可写空间
    vec[0].iov_base = const_cast<char*>(begin()) + m_writerIndex;
    vec[0].iov_len  = writeable;
    // 将vec[1]指向extraBuffer
    vec[1].iov_base = extraBuffer;
    vec[1].iov_len  = sizeof(extraBuffer);
    // 如果可写空间小于extraBuffer，则使用两个iov
    const int     iovcnt = (writeable < sizeof(extraBuffer)) ? 2 : 1;
    const ssize_t n      = ::readv(fd, vec, iovcnt);
    if (n < 0) {
        if (savedErrno != nullptr) {
            *savedErrno = errno;
        }
    } else if (static_cast<size_t>(n) <= writeable) {
        // 原本buffer的可写空间已经足够
        m_writerIndex += n;
    } else {
        // 原本buffer的可写空间不够，需要扩容
        m_writerIndex = m_buffer.size();
        write(extraBuffer, n - writeable);
    }
    return n;
}

ssize_t Buffer::writeFD(const int fd, int* savedErrno) const {
    const ssize_t n = ::write(fd, peek(), getReadableBytes());
    if (n < 0 && savedErrno != nullptr) {
        *savedErrno = errno;
    }
    return n;
}

#ifdef ZMUDUO_ENABLE_OPENSSL
int Buffer::readSSL(SSL* ssl, int* savedErrno) {
    ensureWritableBytes(1024);
    const int n = ::SSL_read(ssl, const_cast<char*>(begin()) + m_writerIndex,
                             static_cast<int>(getWriteableBytes()));
    if (n > 0) {
        m_writerIndex += n;
    } else if (savedErrno != nullptr) {
        *savedErrno = errno;
    }
    return n;
}

int Buffer::writeSSL(SSL* ssl, int* savedErrno) const {
    const int n = ::SSL_write(ssl, peek(), static_cast<int>(getReadableBytes()));
    if (n < 0 && savedErrno) {
        *savedErrno = errno;
    }
    return n;
}
#endif

void Buffer::makeSpace(const size_t length) {
    // 检查当前可写字节和可前置字节是否足够容纳新数据加上偏移量
    if (getWriteableBytes() + getPrependableBytes() < length + m_prependSize) {
        // 如果空间不足，直接调整缓冲区大小
        m_buffer.resize(m_writerIndex + length);
    } else {
        // 确保偏移量小于读索引，以避免覆盖有效数据
        assert(m_prependSize < m_readerIndex);
        // 获取当前可读字节数
        const size_t readableBytes = getReadableBytes();
        // 将未读取区域的字节移动到缓冲区的前置区域
        const auto beginPtr = const_cast<char*>(begin());
        std::copy(beginPtr + m_readerIndex, beginPtr + m_writerIndex, beginPtr + m_prependSize);
        // 更新读索引和写索引
        m_readerIndex = m_prependSize;
        m_writerIndex = m_readerIndex + readableBytes;
        // 确保可读字节数未发生变化
        assert(readableBytes == getReadableBytes());
    }
}
} // namespace zmuduo::net