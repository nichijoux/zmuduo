// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_CHANNEL_H
#define ZMUDUO_NET_CHANNEL_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/base/timestamp.h"
#include "zmuduo/net/callbacks.h"
#include <functional>
#include <memory>
#include <sys/epoll.h>

namespace zmuduo::net {
// forward declaration
class EventLoop;

/**
 * @class Channel
 * @brief Channel类用于将文件描述符fd及其感兴趣的事件注册到Poller中，并在事件发生时调用相应回调
 *
 * @note Channel不拥有fd，只是一个事件分发器
 * @note 一个Channel对应一个文件描述符及其感兴趣的事件
 */
class Channel : NoCopyable {
public:
    /**
     * @brief 构造函数
     * @param[in] eventLoop 所属事件循环对象
     * @param[in] fd 绑定的文件描述符
     */
    Channel(EventLoop* eventLoop, int fd);

    /**
     * @brief 析构函数，默认实现
     */
    ~Channel() = default;

    /**
     * @brief 处理Poller返回的事件，分发到对应的回调
     * @param[in] receiveTime 事件接收时间戳
     */
    void handleEvent(const Timestamp& receiveTime) const;

    /**
     * @brief 设置读取事件的回调函数
     * @param[in] cb 读取事件的回调函数
     */
    void setReadCallback(ReadEventCallback cb) {
        m_readCallback = std::move(cb);
    }

    /**
     * @brief 设置写入事件的回调函数
     * @param[in] cb 写入事件的回调函数
     */
    void setWriteCallback(EventCallback cb) {
        m_writeCallback = std::move(cb);
    }

    /**
     * @brief 设置关闭事件的回调函数
     * @param[in] cb 关闭事件的回调函数
     */
    void setCloseCallback(EventCallback cb) {
        m_closeCallback = std::move(cb);
    }

    /**
     * @brief 设置错误事件的回调函数
     * @param[in] cb 错误事件的回调函数
     */
    void setErrorCallback(EventCallback cb) {
        m_errorCallback = std::move(cb);
    }

    /**
     * @brief 绑定资源所有权，防止资源在回调时被提前释放
     * @param[in] object 被绑定的shared_ptr对象
     * @note 用于解决对象生命周期问题，防止handleEvent过程中对象析构
     */
    void tie(const std::shared_ptr<void>& object);

    /**
     * @brief 设置Channel在Poller中的状态索引
     * @param[in] index 状态索引（用于Poller中记录Channel位置）
     */
    void setIndex(const int index) {
        m_index = index;
    }

    /**
     * @brief 设置Poller返回的实际发生的事件
     * @param[in] happenedEvents 实际发生的事件位掩码
     */
    void setHappenedEvents(const uint32_t happenedEvents) {
        m_happenedEvents = happenedEvents;
    }

    /**
     * @brief 启用读取事件监听
     * @note 会调用update更新Poller注册事件
     */
    void enableReading() {
        if (!isReading()) {
            m_events |= S_READ_EVENT;
            update();
        }
    }

    /**
     * @brief 禁用读取事件监听
     */
    void disableReading() {
        if (isReading()) {
            m_events &= ~S_READ_EVENT;
            update();
        }
    }

    /**
     * @brief 启用写入事件监听
     * @note 会调用update更新Poller注册事件
     */
    void enableWriting() {
        if (!isWriting()) {
            m_events |= S_WRITE_EVENT;
            update();
        }
    }

    /**
     * @brief 禁用写入事件监听
     */
    void disableWriting() {
        if (isWriting()) {
            m_events &= ~S_WRITE_EVENT;
            update();
        }
    }

    /**
     * @brief 同时启用读写事件监听
     */
    void enableAll() {
        if (!isReading() || !isWriting()) {
            m_events |= S_WRITE_EVENT | S_READ_EVENT;
            update();
        }
    }

    /**
     * @brief 禁用所有事件监听
     */
    void disableAll() {
        if (isReading() || isWriting()) {
            m_events = S_NONE_EVENT;
            update();
        }
    }

    /**
     * @brief 检查是否没有监听任何事件
     * @retval true 未监听事件
     * @retval false 有事件监听
     */
    bool isNoneEvent() const {
        return m_events == S_NONE_EVENT;
    }

    /**
     * @brief 检查是否监听读取事件
     * @retval true 正在监听读取事件
     * @retval false 未监听读取事件
     */
    bool isReading() const {
        return m_events & S_READ_EVENT;
    }

    /**
     * @brief 检查是否监听写入事件
     * @retval true 正在监听写入事件
     * @retval false 未监听写入事件
     */
    bool isWriting() const {
        return m_events & S_WRITE_EVENT;
    }

    /**
     * @brief 获取所属的事件循环对象
     * @return 所属EventLoop指针
     */
    EventLoop* getOwnerLoop() const {
        return m_ownerLoop;
    }

    /**
     * @brief 获取绑定的文件描述符
     */
    int getFD() const {
        return m_fd;
    }

    /**
     * @brief 获取Poller中的状态索引
     */
    int getIndex() const {
        return m_index;
    }

    /**
     * @brief 获取感兴趣的事件位掩码
     */
    uint32_t getEvents() const {
        return m_events;
    }

    /**
     * @brief 在所属事件循环中注销本Channel
     */
    void remove();

private:
    /**
     * @brief 更新事件监听，在Poller中注册或修改事件
     * @note 封装了对epoll_ctl的操作
     */
    void update();

    /**
     * @brief 带保护的事件处理函数，保证资源未被释放
     * @param[in] receiveTime 接收时间戳
     */
    void handleEventWithGuard(const Timestamp& receiveTime) const;

private:
    static constexpr int S_NONE_EVENT  = 0;                  ///< 不监听任何事件
    static constexpr int S_READ_EVENT  = EPOLLIN | EPOLLPRI; ///< 读事件（包括普通读、带外数据）
    static constexpr int S_WRITE_EVENT = EPOLLOUT;           ///< 写事件

private:
    EventLoop* m_ownerLoop;           ///< 所属的事件循环对象
    const int  m_fd;                  ///< 被监听的文件描述符
    uint32_t   m_events         = 0;  ///< 当前注册的事件集合
    uint32_t   m_happenedEvents = 0;  ///< 实际发生的事件集合，由Poller填写
    int        m_index          = -1; ///< Channel在Poller中的状态（如数组下标或标志）

    std::weak_ptr<void> m_tie;          ///< 弱引用绑定对象（如TcpConnection）
    bool                m_tied = false; ///< 是否已经绑定对象

    ReadEventCallback m_readCallback;  ///< 读事件回调函数
    EventCallback     m_writeCallback; ///< 写事件回调函数
    EventCallback     m_closeCallback; ///< 关闭事件回调函数
    EventCallback     m_errorCallback; ///< 错误事件回调函数
};
} // namespace zmuduo::net

#endif