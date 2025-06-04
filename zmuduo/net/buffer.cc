#include "zmuduo/net/buffer.h"
#include <algorithm>
#include <cerrno>
#include <sys/uio.h>
#include <unistd.h>

namespace zmuduo::net {
void Buffer::retrieve(size_t length) {
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

std::string Buffer::retrieveAsString(size_t length) {
    assert(length <= getReadableBytes());
    std::string result(peek(), length);
    retrieve(length);
    return result;
}

#define peekData(type)                                                                             \
    do {                                                                                           \
        assert(getReadableBytes() >= sizeof(type));                                                \
        type data;                                                                                 \
        memcpy(&data, peek(), sizeof(data));                                                       \
        return static_cast<type>(byteSwapOnLittleEndian(data));                                    \
    } while (false)

int64_t Buffer::peekInt64() const {
    peekData(int64_t);
}

int32_t Buffer::peekInt32() const {
    peekData(int32_t);
}

int16_t Buffer::peekInt16() const {
    peekData(int64_t);
}

int8_t Buffer::peekInt8() const {
    peekData(int64_t);
}

#undef peekData

void Buffer::prepend(const void* data, size_t length) {
    assert(length <= getPrependableBytes());
    m_readerIndex -= length;
    const char* ptr = static_cast<const char*>(data);
    std::copy(ptr, ptr + length, const_cast<char*>(begin()) + m_readerIndex);
}

#define prependData(x)                                                                             \
    do {                                                                                           \
        auto data = byteSwapOnLittleEndian(x);                                                     \
        prepend(&data, sizeof(data));                                                              \
    } while (false)

void Buffer::prependInt64(int64_t x) {
    prependData(x);
}

void Buffer::prependInt32(int32_t x) {
    prependData(x);
}

void Buffer::prependInt16(int16_t x) {
    prependData(x);
}

void Buffer::prependInt8(int8_t x) {
    prependData(x);
}

#undef prependData

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

void Buffer::write(const void* data, size_t length) {
    ensureWritableBytes(length);
    const char* ptr = static_cast<const char*>(data);
    std::copy(ptr, ptr + length, const_cast<char*>(getBeginWrite()));
    hasWritten(length);
}

#define writeData(x)                                                                               \
    do {                                                                                           \
        decltype(x) data = byteSwapOnLittleEndian(x);                                              \
        write(reinterpret_cast<const char*>(&data), sizeof(data));                                 \
    } while (false)

void Buffer::writeInt64(int64_t x) {
    writeData(x);
}

void Buffer::writeInt32(int32_t x) {
    writeData(x);
}

void Buffer::writeInt16(int16_t x) {
    writeData(x);
}

void Buffer::writeInt8(int8_t x) {
    writeData(x);
}

#undef writeData

void Buffer::read(void* dist, size_t length) {
    assert(getReadableBytes() >= length);
    memcpy(dist, peek(), length);
    retrieve(length);
}

#define readData(type)                                                                             \
    do {                                                                                           \
        assert(getReadableBytes() >= sizeof(type));                                                \
        type data;                                                                                 \
        memcpy(&data, peek(), sizeof(data));                                                       \
        data = static_cast<type>(byteSwapOnLittleEndian(data));                                    \
        retrieve(sizeof(type));                                                                    \
        return data;                                                                               \
    } while (false)

int64_t Buffer::readInt64() {
    readData(int64_t);
}

int32_t Buffer::readInt32() {
    readData(int32_t);
}

int16_t Buffer::readInt16() {
    readData(int16_t);
}

int8_t Buffer::readInt8() {
    readData(int8_t);
}
#undef readData

void Buffer::unwrite(size_t length) {
    assert(length <= getReadableBytes());
    m_readerIndex -= length;
}

ssize_t Buffer::readFD(int fd, int* savedErrno) {
    char extraBuffer[65536] = {0};
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

ssize_t Buffer::writeFD(int fd, int* savedErrno) const {
    const ssize_t n = ::write(fd, peek(), getReadableBytes());
    if (n < 0 && savedErrno != nullptr) {
        *savedErrno = errno;
    }
    return n;
}

#ifdef ZMUDUO_ENABLE_OPENSSL
int Buffer::readSSL(SSL* ssl, int* savedErrno) {
    ensureWritableBytes(1024);
    int n = ::SSL_read(ssl, reinterpret_cast<void*>(const_cast<char*>(begin()) + m_writerIndex),
                       static_cast<int>(getWriteableBytes()));
    if (n > 0) {
        m_writerIndex += n;
    } else if (savedErrno != nullptr) {
        *savedErrno = errno;
    }
    return n;
}

int Buffer::writeSSL(SSL* ssl, int* savedErrno) const {
    int n = ::SSL_write(ssl, peek(), static_cast<int>(getReadableBytes()));
    if (n < 0 && savedErrno) {
        *savedErrno = errno;
    }
    return n;
}
#endif

void Buffer::makeSpace(size_t length) {
    // 检查当前可写字节和可前置字节是否足够容纳新数据加上偏移量
    if (getWriteableBytes() + getPrependableBytes() < length + m_prependSize) {
        // 如果空间不足，直接调整缓冲区大小
        m_buffer.resize(m_writerIndex + length);
    } else {
        // 确保偏移量小于读索引，以避免覆盖有效数据
        assert(m_prependSize < m_readerIndex);
        // 获取当前可读字节数
        size_t readableBytes = getReadableBytes();
        // 将未读取区域的字节移动到缓冲区的前置区域
        char* beginPtr = const_cast<char*>(begin());
        std::copy(beginPtr + m_readerIndex, beginPtr + m_writerIndex, beginPtr + m_prependSize);
        // 更新读索引和写索引
        m_readerIndex = m_prependSize;
        m_writerIndex = m_readerIndex + readableBytes;
        // 确保可读字节数未发生变化
        assert(readableBytes == getReadableBytes());
    }
}

}  // namespace zmuduo::net