// Original code - Copyright (c) 2010, Chen Shuo
// Modified by nichijoux

#ifndef ZMUDUO_NET_ACCEPTOR_H
#define ZMUDUO_NET_ACCEPTOR_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/channel.h"
#include "zmuduo/net/socket.h"
#include <functional>

namespace zmuduo::net {

class EventLoop;

/**
 * @class Acceptor
 * @brief 用于在服务端监听新连接请求的接收器类
 *
 * 封装了 socket、channel 和新连接到来时的处理逻辑。<br/>
 * 通常运行在 mainLoop（即 baseLoop）中，负责处理 accept 事件，<br/>
 * 并通过回调函数通知上层创建新的 TcpConnection。
 *
 * @note 每当有新连接到达时，会触发 handleRead，从而调用上层注册的 NewConnectionCallback。
 */
class Acceptor {
  public:
    /**
     * @typedef std::function&lt;void(int sockFD, const Address::Ptr& address)&gt;
     * @brief 新连接建立时的回调函数类型
     * @param[in] sockFD 已建立连接的 socket 文件描述符
     * @param[in] address 对端客户端地址的智能指针
     */
    using NewConnectionCallback = std::function<void(int sockFD, const Address::Ptr& address)>;

  public:
    /**
     * @brief 构造函数
     * @param[in] loop 所属的事件循环（通常为主 EventLoop）
     * @param[in] listenAddr 监听的本地地址（IP + 端口）
     * @param[in] reuseport 是否开启端口复用（SO_REUSEPORT）
     */
    Acceptor(EventLoop* loop, const Address::Ptr& listenAddr, bool reuseport);

    /**
     * @brief 析构函数
     * @note 析构时会关闭监听 socket 和通道资源
     */
    ~Acceptor();

    /**
     * @brief 设置新连接回调函数
     * @param[in] callback 用户提供的处理新连接的回调
     * @note 回调在每次有新连接建立时被调用，需负责接管该 socket fd
     */
    void setNewConnectionCallback(NewConnectionCallback callback) {
        m_connectCallback = std::move(callback);
    }

    /**
     * @brief 开始监听
     * @note 调用 listen 后将注册 channel 可读事件，接收连接
     */
    void listen();

    /**
     * @brief 获取当前是否处于监听状态
     * @retval true 正在监听
     * @retval false 尚未调用 listen()
     */
    bool isListening() const {
        return m_listening;
    }

  private:
    /**
     * @brief 处理可读事件（即有新连接到来）
     * @note 此函数由 m_acceptChannel 可读事件触发，内部执行 accept()
     */
    void handleRead();

  private:
    EventLoop* m_eventLoop;      ///< 所属的事件循环（一般为 mainLoop）
    TcpSocket  m_acceptSocket;   ///< 内部使用的监听 socket 封装对象
    Channel    m_acceptChannel;  ///< 与监听 socket 绑定的 channel，监听可读事件
    bool       m_listening;      ///< 是否处于监听状态
    NewConnectionCallback m_connectCallback;  ///< 新连接建立时的回调函数
    /**
     * @brief 备用空闲文件描述符，用于处理 FD 耗尽情况
     *
     * @details 当系统文件描述符已满（EMFILE）时，临时关闭 idleFD，
     * 释放出一个 fd 来完成 accept()，然后立即 close 该连接，最后重新打开 idleFD。
     * 此技巧可防止客户端因 FD 耗尽而收到连接重置。
     */
    int m_idleFD;  ///< 备用空闲文件描述符，用于处理 FD 耗尽情况
};

}  // namespace zmuduo::net

#endif  // ZMUDUO_NET_ACCEPTOR_H
