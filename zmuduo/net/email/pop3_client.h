// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_EMAIL_POP3_CLIENT_H
#define ZMUDUO_NET_EMAIL_POP3_CLIENT_H

#include "zmuduo/base/nocopyable.h"
#include "zmuduo/net/email/pop3.h"
#include "zmuduo/net/tcp_client.h"
#include "zmuduo/net/uri.h"
#include <functional>
#include <queue>
#include <string>
#include <utility>
#include <variant>

namespace zmuduo::net::email {
/**
 * @class Pop3Client
 * @brief POP3 客户端类，用于连接 POP3 服务器并执行邮件操作。
 *
 * Pop3Client 支持通过 URI 或服务器地址建立 POP3 连接，执行认证（USER, PASS）和命令（STAT, UIDL,
 * RETR, DELE 等）。<br/>
 * 使用状态机管理连接流程（DISCONNECT -> AUTHORIZATION -> TRANSACTION ->
 * UPDATE），通过命令队列和回调队列实现异步操作。
 *
 * @note Pop3Client 不可拷贝（继承自 NoCopyable），通过 EventLoop 确保线程安全。
 * @note 支持 USER/PASS 认证，支持 APOP 认证。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * Pop3Client client(&loop, "pop3://pop.example.com:110", "user@example.com", "password", false,
 * "MyPop3Client"); client.setAuthenticateCallback([]() { std::cout << "Authentication succeeded" <<
 * std::endl; client.stat([](const Pop3StatResponse::Ptr& resp) { if (resp->m_success) { std::cout
 * << "Messages: " << resp->m_num << ", Size: " << resp->m_size << std::endl;
 *          }
 *      });
 * });
 * client.connect();
 * loop.loop();
 * @endcode
 */
class Pop3Client : NoCopyable {
  public:
    /**
     * @brief 认证完成回调
     * @typedef std::function&lt;void()&gt;
     */
    using AuthenticateCallback = std::function<void()>;
    /**
     * @typedef std::function&lt;void(const Pop3Response::Ptr& response)&gt;
     * @brief 命令响应回调
     * @param[in] response 响应内容
     */
    using CommandCallback = std::function<void(const Pop3Response::Ptr& response)>;

  public:
    /**
     * @brief 构造函数，通过 URI 字符串创建客户端。
     * @param[in,out] loop 所属事件循环。
     * @param[in] uri POP3 URI 字符串（如 "pop3://user:password@mail.example.com:110"）。
     * @param[in] useApop 是否使用APOP进行认证（默认为 false）
     * @param[in] name 客户端名称（用于标识和日志记录）。
     * @note uri必须包含username和password
     * @throws std::invalid_argument 若 URI 无效或地址解析失败。
     */
    Pop3Client(EventLoop*         loop,
               const std::string& uri,
               bool               useApop = false,
               std::string        name    = std::string());

    /**
     * @brief 构造函数，通过 Uri 对象创建客户端。
     * @param[in,out] loop 所属事件循环。
     * @param[in] uri URI 实例（需为 pop3 协议）。
     * @param[in] useApop 是否使用APOP进行认证（默认为 false）
     * @param[in] name 客户端名称。
     * @note uri.getUserInfo()必须为username:password形式
     * @throws std::invalid_argument 若 URI 方案不是 pop3 或地址解析失败。
     */
    Pop3Client(EventLoop*  loop,
               const Uri&  uri,
               bool        useApop = false,
               std::string name    = std::string());

    /**
     * @brief 构造函数，通过服务器地址创建客户端。
     * @param[in,out] loop 所属事件循环。
     * @param[in] hostAddress POP3 服务器地址。
     * @param[in] username 用户名。
     * @param[in] password 密码。
     * @param[in] useApop 是否使用APOP进行认证（默认为 false）
     * @param[in] name 客户端名称。
     */
    Pop3Client(EventLoop*          loop,
               const Address::Ptr& hostAddress,
               std::string         username,
               std::string         password,
               bool                useApop = false,
               std::string         name    = std::string()) noexcept;

    /**
     * @brief 析构函数。
     */
    ~Pop3Client() = default;

    /**
     * @brief 启动 POP3 连接。
     * @note 发起 TCP 连接，进入 AUTHORIZATION 状态。
     */
    void connect() {
        m_client.connect();
    }

    /**
     * @brief 断开 POP3 连接。
     * @note 关闭 TCP 连接，重置状态为 DISCONNECT。
     */
    void disconnect() {
        m_client.disconnect();
    }

    /**
     * @brief 停止客户端并释放资源。
     * @note 停止 TCP 客户端，重置状态为 DISCONNECT。
     */
    void stop() {
        m_client.stop();
    }

    /**
     * @brief 设置认证完成回调。
     * @param[in] callback 认证成功后调用的函数。
     */
    void setAuthenticateCallback(AuthenticateCallback callback) {
        m_authenticateCallback = std::move(callback);
    }

#ifdef ZMUDUO_ENABLE_OPENSSL
    /**
     * @brief 加载 SSL 证书和私钥。
     * @param[in] certificatePath 证书文件路径。
     * @param[in] privateKeyPath 私钥文件路径。
     * @param[in] caFile CA 证书文件路径（可选）。
     * @param[in] caPath CA 证书目录路径（可选）。
     * @retval true 成功加载证书和私钥。
     * @retval false 加载证书或私钥失败，或客户端已连接。
     * @note 必须在客户端连接前调用。
     */
    bool loadCertificates(const std::string& certificatePath,
                          const std::string& privateKeyPath,
                          const std::string& caFile = "",
                          const std::string& caPath = "") {
        return m_client.loadCertificates(certificatePath, privateKeyPath, caFile, caPath);
    }
#endif

    /**
     * @brief 执行 STAT 命令，查询邮箱状态。
     * @param[in] callback 响应回调，返回 Pop3StatResponse（邮件总数和总大小）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void stat(std::function<void(const Pop3StatResponse::Ptr&)> callback);

    /**
     * @brief 执行无参数 UIDL 命令，列出所有邮件的唯一标识符。
     * @param[in] callback 响应回调，返回 Pop3UidlResponse（邮件编号和 UID 列表）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void uidl(std::function<void(const Pop3UidlResponse::Ptr&)> callback);

    /**
     * @brief 执行带参数 UIDL 命令，获取指定邮件的唯一标识符。
     * @param[in] num 邮件编号。
     * @param[in] callback 响应回调，返回 Pop3UidlNResponse（邮件编号和 UID）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void uidl(int num, std::function<void(const Pop3UidlNResponse::Ptr&)> callback);

    /**
     * @brief 执行无参数 LIST 命令，列出所有邮件的大小。
     * @param[in] callback 响应回调，返回 Pop3ListResponse（邮件编号和大小列表）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void list(std::function<void(const Pop3ListResponse::Ptr&)> callback);

    /**
     * @brief 执行带参数 LIST 命令，获取指定邮件的大小。
     * @param[in] num 邮件编号。
     * @param[in] callback 响应回调，返回 Pop3ListNResponse（邮件编号和大小）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void list(int num, std::function<void(const Pop3ListNResponse::Ptr&)> callback);

    /**
     * @brief 执行 RETR 命令，检索指定邮件内容。
     * @param[in] num 邮件编号。
     * @param[in] callback 响应回调，返回 Pop3RetrResponse（邮件内容）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void retr(int num, std::function<void(const Pop3RetrResponse::Ptr&)> callback);

    /**
     * @brief 执行 DELE 命令，标记指定邮件为删除。
     * @param[in] num 邮件编号。
     * @param[in] callback 响应回调，返回 Pop3DeleResponse（响应消息）。
     * @note 仅在 TRANSACTION 状态下发送，实际删除在 QUIT 后生效。
     */
    void dele(int num, std::function<void(const Pop3DeleResponse::Ptr&)> callback);

    /**
     * @brief 执行 NOOP 命令，保持连接活跃。
     * @param[in] callback 响应回调，返回 Pop3NoopResponse。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void noop(std::function<void(const Pop3NoopResponse::Ptr&)> callback);

    /**
     * @brief 执行 RSET 命令，重置会话状态。
     * @param[in] callback 响应回调，返回 Pop3RsetResponse（响应消息）。
     * @note 仅在 TRANSACTION 状态下发送，取消所有 DELE 标记。
     */
    void rset(std::function<void(const Pop3RsetResponse::Ptr&)> callback);

    /**
     * @brief 执行 TOP 命令，检索指定邮件的头部和部分正文。
     * @param[in] num 邮件编号。
     * @param[in] line 正文预览行数。
     * @param[in] callback 响应回调，返回 Pop3TopResponse（头部和正文预览）。
     * @note 仅在 TRANSACTION 状态下发送。
     */
    void top(int num, int line, std::function<void(const Pop3TopResponse::Ptr&)> callback);

    /**
     * @brief 执行 QUIT 命令，结束会话。
     * @param[in] callback 响应回调，返回 Pop3QuitResponse（响应消息）。
     * @note 进入 UPDATE 状态，应用 DELE 标记并断开连接。
     */
    void quit(std::function<void(const Pop3QuitResponse::Ptr&)> callback);

  private:
    /**
     * @brief 连接状态枚举。
     */
    enum class State {
        DISCONNECT,     ///< 未连接
        AUTHORIZATION,  ///< 认证中（USER, PASS）
        TRANSACTION,    ///< 事务处理（STAT, UIDL, RETR 等）
        UPDATE          ///< 更新状态（QUIT）
    };

    /**
     * @brief 命令类型枚举。
     */
    enum class Command {
        NONE,    ///< 无命令（初始状态）
        USER,    ///< USER 认证
        PASS,    ///< PASS 认证
        APOP,    ///< APOP 认证（未实现）
        STAT,    ///< 查询邮箱状态
        UIDL,    ///< 列出所有邮件 UID
        UIDL_N,  ///< 获取指定邮件 UID
        LIST,    ///< 列出所有邮件大小
        LIST_N,  ///< 获取指定邮件大小
        RETR,    ///< 检索邮件内容
        DELE,    ///< 标记邮件删除
        RSET,    ///< 重置会话
        TOP,     ///< 检索邮件头部和部分正文
        NOOP,    ///< 空操作
        CAPA,    ///< 查询服务器功能
        QUIT     ///< 结束会话
    };

    /**
     * @brief 处理 TCP 连接建立或断开。
     * @param[in] connection TCP 连接，表示 POP3 会话。
     * @note 连接建立时进入 AUTHORIZATION 状态，断开时重置状态。
     */
    void onConnection(const TcpConnectionPtr& connection);

    /**
     * @brief 处理接收到的数据。
     * @param[in] connection TCP 连接，表示 POP3 会话。
     * @param[in,out] buffer 接收缓冲区。
     * @param[in] receiveTime 接收时间戳。
     * @note 根据状态分发到 handleAuthorization, handleTransaction 或 handleUpdate。
     */
    void
    onMessage(const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime);

    /**
     * @brief 处理认证阶段的响应。
     * @param[in] connection TCP 连接。
     * @param[in,out] buffer 接收缓冲区。
     * @note 处理 USER, PASS 命令响应，成功后进入 TRANSACTION 状态。
     */
    void handleAuthorization(const TcpConnectionPtr& connection, Buffer& buffer);

    /**
     * @brief 处理事务阶段的响应。
     * @param[in] connection TCP 连接。
     * @param[in,out] buffer 接收缓冲区。
     * @note 解析 STAT, UIDL, RETR 等命令响应，触发对应回调。
     */
    void handleTransaction(Buffer& buffer);

    /**
     * @brief 预处理 POP3 响应。
     * @param[in] response 服务器响应字符串。
     * @retval true 响应以 "+OK" 开头，表示成功。
     * @retval false 响应不以 "+OK" 开头，表示失败。
     */
    static bool preprocessing(const std::string& response);

    /**
     * @brief 处理 STAT 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析邮件总数和总大小，触发回调。
     */
    void handleStat(const std::string& response);

    /**
     * @brief 处理无参数 UIDL 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析邮件编号和 UID 列表，触发回调。
     */
    void handleUidl(const std::string& response);

    /**
     * @brief 处理带参数 UIDL 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析指定邮件的编号和 UID，触发回调。
     */
    void handleUidlN(const std::string& response);

    /**
     * @brief 处理无参数 LIST 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析邮件编号和大小列表，触发回调。
     */
    void handleList(const std::string& response);

    /**
     * @brief 处理带参数 LIST 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析指定邮件的编号和大小，触发回调。
     */
    void handleListN(const std::string& response);

    /**
     * @brief 处理 RETR 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析邮件内容，处理透明传输（还原 ".." 为 "."），触发回调。
     */
    void handleRetr(const std::string& response);

    /**
     * @brief 处理 DELE 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析响应消息，触发回调。
     */
    void handleDele(const std::string& response);

    /**
     * @brief 处理 RSET 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析响应消息，触发回调。
     */
    void handleRset(const std::string& response);

    /**
     * @brief 处理 TOP 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析邮件头部和正文预览，触发回调。
     */
    void handleTop(const std::string& response);

    /**
     * @brief 处理 NOOP 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 触发回调，无额外数据。
     */
    void handleNoop(const std::string& response);

    /**
     * @brief 处理 CAPA 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析服务器支持的功能列表，触发回调。
     */
    void handleCapa(const std::string& response);

    /**
     * @brief 处理 QUIT 命令响应。
     * @param[in] response 服务器响应字符串。
     * @note 解析响应消息，进入 UPDATE 状态并断开连接。
     */
    void handleQuit(const std::string& response);

    /**
     * @brief 处理错误情况。
     * @param[in] error 错误描述。
     * @note 记录日志并断开连接。
     */
    void handleError(const std::string& error);

    /**
     * @brief 包装回调函数以支持类型安全的响应。
     * @param[in] cb 用户提供的类型化回调。
     * @return 通用 CommandCallback，执行类型转换后调用 cb。
     * @note 确保回调接收正确的 Pop3Response 派生类。
     */
    template <typename T>
    static CommandCallback wrapCallback(std::function<void(std::shared_ptr<T>)> cb);

  private:
    TcpClient         m_client;    ///< 底层 TCP 客户端
    State             m_state;     ///< 当前连接状态
    const std::string m_username;  ///< 用户名
    const std::string m_password;  ///< 原始密码
    std::string m_finalPassword;  ///< 最终密码（用于APOP的MD5摘要或USER/PASS的明文密码）
    const bool m_useApop;         ///< 是否使用apop方式进行认证
    bool       m_maybeRetry;  ///< 是否需要继续解析消息(消息可能一次性来了很多)
    std::string                 m_timestamp;             ///< 存储服务器返回的时间戳
    std::queue<Command>         m_commands;              ///< 命令队列
    std::queue<CommandCallback> m_callbacks;             ///< 回调队列
    AuthenticateCallback        m_authenticateCallback;  ///< 认证完成回调
};
}  // namespace zmuduo::net::email

#endif