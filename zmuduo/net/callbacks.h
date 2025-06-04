// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_CALLBACKS_H
#define ZMUDUO_NET_CALLBACKS_H

#include "zmuduo/base/timestamp.h"
#include <functional>
#include <memory>

namespace zmuduo::net {

class Buffer;
class TcpConnection;

/**
 * @typedef std::shared_ptr&lt;TcpConnection&gt;
 * @brief TcpConnection 的智能指针类型定义（shared_ptr）
 */
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @typedef std::function&lt;void()&gt;
 * @brief 定时器触发时的回调函数类型
 * @note 无参数、无返回值，常用于超时处理
 */
using TimerCallback = std::function<void()>;

/**
 * @typedef std::function&lt;void()&gt;
 * @brief 泛型事件处理回调函数
 * @note 适用于触发时无参数传入的事件
 */
using EventCallback = std::function<void()>;

/**
 * @typedef std::function&lt;void(c&gt;nst Timestamp&)>
 * @brief 可读事件发生时的回调函数
 * @param[in] Timestamp 数据接收时间戳
 */
using ReadEventCallback = std::function<void(const Timestamp&)>;

/**
 * @typedef std::function&lt;void(c&gt;nst TcpConnectionPtr&)>
 * @brief 新连接建立或连接状态变化时的回调函数
 * @param[in] TcpConnectionPtr 当前连接的共享指针
 */
using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;

/**
 * @typedef std::function&lt;void(c&gt;nst TcpConnectionPtr&)>
 * @brief 发送缓冲区数据全部写入完成后的回调函数
 * @param[in] TcpConnectionPtr 当前连接的共享指针
 */
using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;

/**
 * @typedef std::function&lt;void(c&gt;nst TcpConnectionPtr&)>
 * @brief TCP连接关闭时的回调函数
 * @param[in] TcpConnectionPtr 当前连接的共享指针
 */
using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

/**
 * @typedef std::function&lt;void(c&gt;nst TcpConnectionPtr&, size_t)
 * @brief 写入缓冲区数据量超过高水位线时的回调函数
 * @param[in] TcpConnectionPtr 当前连接的共享指针
 * @param[in] size 当前缓冲区大小（字节）
 * @note 可用于流量控制机制
 */
using HighWaterMarkCallback = std::function<void(const TcpConnectionPtr&, size_t)>;

/**
 * @typedef std::function&lt;void(c&gt;nst TcpConnectionPtr&, Buffer&, const Timestamp&)>
 * @brief 接收到完整消息时的回调函数
 * @param[in] TcpConnectionPtr 当前连接的共享指针
 * @param[in,out] Buffer 接收到的数据缓冲区
 * @param[in] Timestamp 数据接收的时间戳
 */
using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer&, const Timestamp&)>;

/**
 * @brief 默认连接状态变更回调函数
 * @param[in] connection 当前连接的共享指针
 * @note 可作为默认行为使用，通常用于打印日志或简易逻辑处理
 */
void defaultConnectionCallback(const TcpConnectionPtr& connection);

/**
 * @brief 默认消息接收回调函数
 * @param[in] connection 当前连接的共享指针
 * @param[in,out] buffer 接收到的数据缓冲区
 * @param[in] receiveTime 接收数据的时间戳
 * @note 默认行为是丢弃消息或打印日志，可根据需要替换
 */
void defaultMessageCallback(const TcpConnectionPtr& connection,
                            Buffer&                 buffer,
                            const Timestamp&        receiveTime);

}  // namespace zmuduo::net

#endif  // ZMUDUO_NET_CALLBACKS_H
