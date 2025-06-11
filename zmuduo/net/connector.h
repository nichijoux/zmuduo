// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_CONNECTOR_H
#define ZMUDUO_NET_CONNECTOR_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/address.h"
#include <functional>
#include <memory>

namespace zmuduo::net {
class Channel;
class EventLoop;

/**
 * @class Connector
 * @brief 负责发起和管理TCP连接请求，包括连接建立、错误处理和自动重试。
 *
 * Connector 类采用非阻塞连接机制，通过 Channel 监听写事件来判断连接是否成功。
 * 连接成功后，通过 NewConnectionCallback 回调通知调用者，并传递新连接的 socket 文件描述符。
 *
 * @note Connector 内部维护一个状态机（DISCONNECTED, CONNECTING, CONNECTED）来管理连接生命周期。
 * @note 支持自动重试机制，重试间隔会逐渐增加，最长不超过 S_MAX_RETRY_DELAY_MS。
 *
 * @example
 *
 * @code
 * auto connector = std::make_shared<Connector>(loop, serverAddr);
 * connector->setNewConnectionCallback([](int sockfd) {
 *     // 处理新连接逻辑
 * });
 * connector->start();
 * @endcode
 */
class Connector : NoCopyable, public std::enable_shared_from_this<Connector> {
  public:
    /**
     * @typedef std::function&lt;void(int socketFD)&gt;
     * @brief 新连接建立时的回调函数类型，传递新连接的 socket 文件描述符。
     * @param[in] socketFD 新连接的 socket 文件描述符
     */
    using NewConnectionCallback = std::function<void(int socketFD)>;

  public:
    /**
     * @brief 构造函数，初始化 Connector 实例。
     * @param[in] loop 所属的事件循环，用于异步处理连接事件。
     * @param[in] serverAddress 目标服务器地址的智能指针。
     */
    Connector(EventLoop* loop, Address::Ptr serverAddress);

    /**
     * @brief 析构函数，doNothing
     */
    ~Connector();

    /**
     * @brief 设置新连接建立时的回调函数。
     * @param[in] cb 新连接回调函数，接收 socket 文件描述符作为参数。
     */
    void setNewConnectionCallback(NewConnectionCallback cb) {
        m_newConnectionCallback = std::move(cb);
    }

    /**
     * @brief 启动连接流程，以非阻塞方式发起连接。
     * @note 该方法会在事件循环线程中异步执行 startInLoop。
     */
    void start();

    /**
     * @brief 重新启动连接流程，通常用于连接失败后的重试。
     * @note 重置重试延迟并重新发起连接。
     */
    void restart();

    /**
     * @brief 断开连接,然后等待重新连接
     */
    void disconnect();

    /**
     * @brief 停止连接流程，清理资源并取消重试。
     * @note 该方法会在事件循环线程中异步执行 stopInLoop。
     */
    void stop();

    /**
     * @brief 获取当前的服务器地址
     * @return 服务器地址智能指针
     */
    Address::Ptr getServerAddress() const {
        return m_serverAddress;
    }

  private:
    /**
     * @enum State
     * @brief 表示 Connector 的连接状态。
     */
    enum class State {
        DISCONNECTED,  ///< 断开连接
        CONNECTING,    ///< 连接中
        CONNECTED      ///< 成功连接
    };

    static constexpr int S_MAX_RETRY_DELAY_MS = 30 * 1000;  ///< 最大重试延迟时间（毫秒）
    static constexpr int S_INIT_RETRY_DELAY_MS = 500;  ///< 初始重试延迟时间（毫秒）

  private:
    /**
     * @brief 在事件循环线程中启动连接流程。
     * @note 确保在事件循环线程中调用，检查连接状态并调用 connect。
     */
    void startInLoop();

    /**
     * @brief 在事件循环线程中停止连接流程。
     * @note 清理 Channel 并重置状态为 DISCONNECTED。
     */
    void stopInLoop();

    /**
     * @brief 创建非阻塞 socket 并发起连接。
     * @note 根据 connect 的返回值处理不同情况（如 EINPROGRESS、错误等）。
     */
    void connect();

    /**
     * @brief 处理正在连接中的 socket，设置 Channel 监听写事件。
     * @param[in] socketFD 新创建的 socket 文件描述符。
     */
    void connecting(int socketFD);

    /**
     * @brief 处理 Channel 的写事件，检查连接是否成功。
     * @note 若连接失败，通过 getsockopt 获取错误码并触发重试。
     */
    void handleWrite();

    /**
     * @brief 处理 Channel 的错误事件。
     * @note 记录错误信息并触发重试。
     */
    void handleError();

    /**
     * @brief 触发重试逻辑，关闭当前 socket 并延迟后重新连接。
     * @param[in] socketFD 当前的 socket 文件描述符。
     */
    void retry(int socketFD);

    /**
     * @brief 移除并重置 Channel，获取当前 socket 文件描述符。
     * @return 当前的 socket 文件描述符。
     */
    int removeAndResetChannel();

    /**
     * @brief 重置 Channel 智能指针。
     * @note 在事件循环线程中异步调用以确保安全。
     */
    void resetChannel();

  private:
    EventLoop*               m_eventLoop;              ///< 所属事件循环
    Address::Ptr             m_serverAddress;          ///< 服务器地址
    bool                     m_connect;                ///< 是否启动连接
    State                    m_state;                  ///< 当前连接状态
    std::unique_ptr<Channel> m_channel;                ///< 负责连接事件监听的Channel
    NewConnectionCallback    m_newConnectionCallback;  ///< 新连接回调
    int                      m_retryDelayMs;           ///< 当前重试延迟时间（毫秒）
};
}  // namespace zmuduo::net

#endif