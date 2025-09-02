// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_BUFFER_H
#define ZMUDUO_NET_BUFFER_H

#include "zmuduo/base/copyable.h"
#include "zmuduo/net/endian.h"
#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#ifdef ZMUDUO_ENABLE_OPENSSL
#    include <openssl/ssl.h>
#endif

namespace zmuduo::net {
/**
 * @class Buffer
 * @brief 高性能网络缓冲区，参考org.jboss.netty.buffer.ChannelBuffer设计
 *
 * 提供高效的内存管理，支持读写操作，自动扩容，适用于网络通信场景。<br/>
 * 采用三区段设计，优化内存使用效率：
 * @code
 * +-------------------+------------------+------------------+
 * | prependable bytes |  readable bytes  |  writable bytes  |
 * |                   |     (CONTENT)    |                  |
 * +-------------------+------------------+------------------+
 * |                   |                  |                  |
 * 0      <=      readerIndex   <=   writerIndex    <=     size
 * @endcode
 *
 * @note 继承自Copyable，可拷贝
 * @note 线程安全性：非线程安全，应在同一线程内使用
 */
class Buffer : public Copyable {
public:
    static constexpr size_t S_CHEAP_PREPEND = 8;          ///< 默认预留前缀空间大小
    static constexpr size_t S_INITIAL_SIZE  = 1024;       ///< 初始缓冲区大小
    static constexpr auto   S_CRLF          = "\r\n";     ///< CRLF行结束符
    static constexpr auto   S_HEADER_FOOTER = "\r\n\r\n"; ///< HTTP头结束标记

public:
    /**
     * @brief 构造函数
     * @param[in] prependSize 初始预留前缀空间大小，默认为S_CHEAP_PREPEND
     * @param[in] initialSize 初始缓冲区大小，默认为S_INITIAL_SIZE
     * @note 实际分配空间为initialSize + prependSize
     */
    explicit Buffer(const size_t prependSize = S_CHEAP_PREPEND,
                    const size_t initialSize = S_INITIAL_SIZE)
        : m_buffer(prependSize + initialSize),
          m_prependSize(prependSize),
          m_readerIndex(prependSize),
          m_writerIndex(prependSize) {
        assert(getReadableBytes() == 0);
        assert(getWriteableBytes() == initialSize);
        assert(getPrependableBytes() == prependSize);
    }

    /**
     * @brief 默认析构函数
     */
    ~Buffer() = default;

    /**
     * @brief 重置缓冲区
     * @note 清空内容并恢复初始状态，保留容量
     */
    void reset() {
        m_buffer.clear();
        m_buffer.resize(S_INITIAL_SIZE);
        m_readerIndex = m_writerIndex = m_prependSize;
    }

    /**
     * @brief 交换两个缓冲区内容
     * @param[in,out] other 要交换的另一个缓冲区
     */
    void swap(Buffer& other) noexcept {
        m_buffer.swap(other.m_buffer);
        std::swap(m_readerIndex, other.m_readerIndex);
        std::swap(m_writerIndex, other.m_writerIndex);
    }

    /**
     * @brief 获取可读字节数
     * @return size_t 可读取的字节数
     */
    size_t getReadableBytes() const {
        return m_writerIndex - m_readerIndex;
    }

    /**
     * @brief 获取可写字节数
     * @return size_t 可写入的字节数
     */
    size_t getWriteableBytes() const {
        return m_buffer.size() - m_writerIndex;
    }

    /**
     * @brief 获取可前置字节数
     * @return size_t 可前置的字节数
     */
    size_t getPrependableBytes() const {
        return m_readerIndex;
    }

    /**
     * @brief 获取缓冲区容量
     * @return size_t 缓冲区总容量
     */
    size_t getCapacity() const {
        return m_buffer.capacity();
    }

    /**
     * @brief 查找指定字符串的位置
     * @param target 要查找的目标字符串
     * @return 指向目标字符串的指针，未找到返回nullptr
     */
    const char* find(const char* target) const {
        if (target == nullptr) {
            return nullptr;
        }
        const char* p = std::search(peek(), getBeginWrite(), target, target + strlen(target));
        return p != getBeginWrite() ? p : nullptr;
    }

    /**
     * @brief 查找CRLF位置
     * @return const char* 指向CRLF的指针，未找到返回nullptr
     */
    const char* findCRLF() const {
        return find(S_CRLF);
    }

    /**
     * @brief 查找HTTP头结束标记
     * @return const char* 指向标记的指针，未找到返回nullptr
     */
    const char* findHeaderFooter() const {
        return find(S_HEADER_FOOTER);
    }

    /**
     * @brief 查找行结束符
     * @return const char* 指向EOL的指针，未找到返回nullptr
     */
    const char* findEOL() const {
        auto* eol = memchr(peek(), '\n', getReadableBytes());
        return static_cast<const char*>(eol);
    }

    /**
     * @brief 获取可读数据起始指针
     * @return const char* 指向可读数据的常量指针
     */
    const char* peek() const {
        return begin() + m_readerIndex;
    }

    int64_t peekInt64() const;

    int32_t peekInt32() const;

    int16_t peekInt16() const;

    int8_t peekInt8() const;

    void prepend(const void* data, size_t length);

    void prependInt64(int64_t x);

    void prependInt32(int32_t x);

    void prependInt16(int16_t x);

    void prependInt8(int8_t x);

    /**
     * @brief 从缓冲区取出数据
     * @param[in] length 要取出的字节数
     * @throw std::out_of_range 如果length大于可读字节数
     */
    void retrieve(size_t length);

    /**
     * @brief 取出数据直到指定位置
     * @param[in] end 数据结束位置指针
     */
    void retrieveUntil(const char* end) {
        assert(peek() <= end);
        assert(end <= getBeginWrite());
        retrieve(end - peek());
    }

    void retrieveInt64();

    void retrieveInt32();

    void retrieveInt16();

    void retrieveInt8();

    /**
     * @brief 取出所有数据
     * @note 重置读写指针到初始位置，不清除内容
     */
    void retrieveAll() {
        m_readerIndex = m_writerIndex = m_prependSize;
    }

    /**
     * @brief 取出指定长度数据为字符串
     * @param[in] length 要取出的字节数
     * @return std::string 包含取出数据的字符串
     */
    std::string retrieveAsString(size_t length);

    /**
     * @brief 取出所有可读数据为字符串
     * @return std::string 包含所有可读数据的字符串
     */
    std::string retrieveAllAsString() {
        return retrieveAsString(getReadableBytes());
    }

    /**
     * @brief 确保足够的可写空间
     * @param[in] length 需要的最小可写字节数
     * @note 空间不足时会自动扩容
     */
    void ensureWritableBytes(const size_t length) {
        if (getWriteableBytes() < length) {
            makeSpace(length);
        }
        assert(getWriteableBytes() >= length);
    }

    /**
     * @brief 获取可写位置指针
     * @return const char* 指向可写区域的常量指针
     */
    const char* getBeginWrite() const {
        return begin() + m_writerIndex;
    }

    /**
     * @brief 标记已写入数据
     * @param[in] length 已写入的字节数
     */
    void hasWritten(const size_t length) {
        assert(length <= getWriteableBytes());
        m_writerIndex += length;
    }

    /**
     * @brief 写入数据
     * @param[in] data 要写入的数据指针
     * @param[in] length 要写入的数据长度
     */
    void write(const void* data, size_t length);

    /**
     * @brief 写入字符串
     * @param[in] s 要写入的字符串
     */
    void write(const std::string& s) {
        write(s.data(), s.size());
    }

    void writeInt64(int64_t x);

    void writeInt32(int32_t x);

    void writeInt16(int16_t x);

    void writeInt8(int8_t x);

    /**
     * @brief 读取数据到目标缓冲区
     * @param[out] dist 目标缓冲区指针
     * @param[in] length 要读取的字节数
     */
    void read(void* dist, size_t length);

    int64_t readInt64();

    int32_t readInt32();

    int16_t readInt16();

    int8_t readInt8();

    /**
     * @brief 回退已写入数据
     * @param[in] length 要回退的字节数
     */
    void unwrite(size_t length);

    /**
     * @brief 从文件描述符读取数据
     * @param[in] fd 文件描述符
     * @param[out] savedErrno 保存错误码
     * @return ssize_t 读取的字节数，-1表示错误
     */
    ssize_t readFD(int fd, int* savedErrno);

    /**
     * @brief 写入数据到文件描述符
     * @param[in] fd 文件描述符
     * @param[out] savedErrno 保存错误码
     * @return ssize_t 写入的字节数，-1表示错误
     */
    ssize_t writeFD(int fd, int* savedErrno) const;

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 从SSL连接读取数据
     * @param[in] ssl SSL连接对象
     * @param[out] savedErrno 保存错误码
     * @return int 读取的字节数，<=0表示错误或关闭
     */
    int readSSL(SSL* ssl, int* savedErrno);

    /**
     * @brief 写入数据到SSL连接
     * @param[in] ssl SSL连接对象
     * @param[out] savedErrno 保存错误码
     * @return int 写入的字节数，<=0表示错误或关闭
     */
    int writeSSL(SSL* ssl, int* savedErrno) const;
#endif

private:
    /**
     * @brief 获取内部缓冲区起始指针
     * @return const char* 缓冲区起始指针
     */
    const char* begin() const {
        return m_buffer.data();
    }

    /**
     * @brief 获取内部缓冲区结束指针
     * @return const char* 缓冲区结束指针
     */
    const char* end() const {
        return &m_buffer.back();
    }

    /**
     * @brief 扩容缓冲区
     * @param[in] length 需要的最小额外空间
     * @note 内部使用，自动处理内存移动和扩容
     */
    void makeSpace(size_t length);

private:
    std::vector<char> m_buffer;      ///< 数据存储缓冲区
    size_t            m_prependSize; ///< 前面预留的大小
    size_t            m_readerIndex; ///< 读指针位置
    size_t            m_writerIndex; ///< 写指针位置
};
} // namespace zmuduo::net

#endif