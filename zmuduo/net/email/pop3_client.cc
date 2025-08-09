#include "zmuduo/net/email/pop3_client.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/common_util.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/base/utils/string_util.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/email/email.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/tcp_connection.h"
#include <sstream>
#include <string>
#include <utility>

using namespace zmuduo::utils::hash_util;
using namespace zmuduo::utils::string_util;

namespace {
std::vector<std::string> ensureUserInfoLength(const std::string& userinfo, size_t length) {
    auto parts = Split(userinfo, ':');
    if (parts.size() < length) {
        parts.resize(length, "");  // 如果数组长度不足，用空字符串填充
    }
    return parts;
}
}  // namespace

namespace zmuduo::net::email {
using namespace zmuduo::utils;

Pop3Client::Pop3Client(EventLoop* loop, const std::string& uri, bool useApop, std::string name)
    : Pop3Client(loop, common_util::CheckNotNull(Uri::Create(uri)), useApop, std::move(name)) {}

Pop3Client::Pop3Client(EventLoop* loop, const Uri& uri, bool useApop, std::string name)
    : Pop3Client(loop,
                 uri.createAddress(),
                 ensureUserInfoLength(uri.getUserinfo(), 2)[0],
                 ensureUserInfoLength(uri.getUserinfo(), 2)[1],
                 useApop,
                 std::move(name)) {
    assert(!m_username.empty());
    assert(!m_password.empty());
    assert(uri.getScheme() == "pop3");
#ifdef ZMUDUO_ENABLE_OPENSSL
    m_client.setSSLHostName(uri.getHost());
#endif
}

Pop3Client::Pop3Client(EventLoop*          loop,
                       const Address::Ptr& hostAddress,
                       std::string         username,
                       std::string         password,
                       bool                useApop,
                       std::string         name) noexcept
    : m_client(loop, hostAddress, std::move(name)),
      m_state(State::DISCONNECT),
      m_authenticateCallback(nullptr),
      m_username(std::move(username)),
      m_password(std::move(password)),
      m_useApop(useApop),
      m_maybeRetry(false),
      m_finalPassword(useApop ? "" : m_password),  // 如果使用APOP，m_final_password稍后设置
      m_timestamp(),
      m_commands(),
      m_callbacks() {
    // 设置连接回调
    m_client.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    // 设置消息回调
    m_client.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void Pop3Client::onConnection(const TcpConnectionPtr& connection) {
    if (connection->isConnected()) {
        // 连接成功,发生数据进入认证状态
        m_state = State::AUTHORIZATION;
        m_commands.emplace(Command::NONE);
    }
}

void Pop3Client::onMessage(const TcpConnectionPtr& connection,
                           Buffer&                 buffer,
                           const Timestamp&        receiveTime) {
    switch (m_state) {
        case State::AUTHORIZATION:
            // 认证状态
            handleAuthorization(connection, buffer);
            break;
        case State::TRANSACTION: handleTransaction(buffer); break;
        case State::UPDATE: connection->shutdown(); break;
        case State::DISCONNECT: break;
    }
}

void Pop3Client::handleAuthorization(const TcpConnectionPtr& connection, Buffer& buffer) {
    auto crlf = buffer.findCRLF();
    if (crlf) {
        std::string response(buffer.peek(), crlf);
        buffer.retrieveUntil(crlf + strlen(Buffer::S_CRLF));
#define CHECK_AND_SEND(targetCommand, message)                                                     \
    do {                                                                                           \
        if (preprocessing(response)) {                                                             \
            m_commands.emplace(targetCommand);                                                     \
            connection->send(message);                                                             \
        } else {                                                                                   \
            handleError("Authentication failed: " + response);                                     \
        }                                                                                          \
    } while (false)
        Command command = m_commands.front();
        m_commands.pop();
        switch (command) {
            case Command::NONE: {
                // 解析服务器问候消息中的时间戳（用于APOP）
                if (m_useApop) {
                    size_t start = response.find('<');
                    size_t end   = response.find('>');
                    if (start != std::string::npos && end != std::string::npos && end > start) {
                        m_timestamp     = response.substr(start, end - start + 1);
                        m_finalPassword = hash_util::MD5(m_timestamp + m_password);
                        CHECK_AND_SEND(Command::APOP,
                                       "APOP " + m_username + " " + m_finalPassword + "\r\n");
                    } else {
                        handleError("No valid timestamp for APOP: " + response);
                    }
                } else {
                    CHECK_AND_SEND(Command::USER, "USER " + m_username + "\r\n");
                }
                break;
            }
            case Command::USER: {
                CHECK_AND_SEND(Command::PASS, "PASS " + m_finalPassword + "\r\n");
                break;
            }
            case Command::APOP:  // APOP认证直接进入TRANSACTION状态
            case Command::PASS:
                if (preprocessing(response)) {
                    m_state = State::TRANSACTION;
                    // 认证通过
                    if (m_authenticateCallback) {
                        m_authenticateCallback();
                    }
                } else {
                    handleError(response);
                }
                break;
            default: handleError("意外响应命令: "); break;
        }
    }
#undef CHECK_AND_SEND
}

void Pop3Client::handleTransaction(Buffer& buffer) {
#define CHECK_AND_HANDLE(command, findStr, handleFunc, type)                                       \
    case command: {                                                                                \
        auto str = buffer.find(findStr);                                                           \
        if (str) {                                                                                 \
            m_commands.pop();                                                                      \
            std::string response(buffer.peek(), str);                                              \
            buffer.retrieveUntil(str + strlen(findStr));                                           \
            m_maybeRetry = buffer.getReadableBytes() > 0;                                          \
            if (preprocessing(response)) {                                                         \
                handleFunc(response);                                                              \
            } else {                                                                               \
                CommandCallback callback = m_callbacks.front();                                    \
                m_callbacks.pop();                                                                 \
                type instance;                                                                     \
                instance.m_success = false;                                                        \
                instance.m_message = response;                                                     \
                callback(std::make_shared<type>(instance));                                        \
            }                                                                                      \
        } else {                                                                                   \
            m_maybeRetry = false;                                                                  \
        }                                                                                          \
        break;                                                                                     \
    }
    do {
        // 获取当前command对应的命令,不需要加锁,因为只会在onMessage中pop
        Command command = m_commands.front();
        switch (command) {
            CHECK_AND_HANDLE(Command::STAT, Buffer::S_CRLF, handleStat, Pop3StatResponse)
            CHECK_AND_HANDLE(Command::UIDL, ZMUDUO_EMAIL_END_TAG, handleUidl, Pop3UidlResponse)
            CHECK_AND_HANDLE(Command::UIDL_N, Buffer::S_CRLF, handleUidlN, Pop3UidlNResponse)
            CHECK_AND_HANDLE(Command::LIST, ZMUDUO_EMAIL_END_TAG, handleList, Pop3ListResponse)
            CHECK_AND_HANDLE(Command::LIST_N, Buffer::S_CRLF, handleListN, Pop3ListNResponse)
            CHECK_AND_HANDLE(Command::RETR, ZMUDUO_EMAIL_END_TAG, handleRetr, Pop3RetrResponse)
            CHECK_AND_HANDLE(Command::DELE, Buffer::S_CRLF, handleDele, Pop3DeleResponse)
            CHECK_AND_HANDLE(Command::RSET, Buffer::S_CRLF, handleRset, Pop3RsetResponse)
            CHECK_AND_HANDLE(Command::TOP, ZMUDUO_EMAIL_END_TAG, handleTop, Pop3TopResponse)
            CHECK_AND_HANDLE(Command::NOOP, Buffer::S_CRLF, handleNoop, Pop3NoopResponse)
            CHECK_AND_HANDLE(Command::CAPA, ZMUDUO_EMAIL_END_TAG, handleCapa, Pop3CapaResponse)
            CHECK_AND_HANDLE(Command::QUIT, Buffer::S_CRLF, handleQuit, Pop3QuitResponse)
            default:
                handleError("command类型错误" + std::to_string(static_cast<int>(command)));
                break;
        }
    } while (m_maybeRetry);
#undef CHECK_AND_HANDLE
}

bool Pop3Client::preprocessing(const std::string& response) {
    ZMUDUO_LOG_DEBUG << "POP3 响应: " << response;
    // 检查响应是否以 "+OK" 开头
    return StartsWith(response, "+OK");
}

#define GET_AND_CALL_CALLBACK(response)                                                            \
    do {                                                                                           \
        CommandCallback callback = m_callbacks.front();                                            \
        m_callbacks.pop();                                                                         \
        callback(response);                                                                        \
    } while (false)

void Pop3Client::handleStat(const std::string& response) {
    // 解析 STAT 响应（+OK <邮件总数> <总字节数>）
    std::istringstream ss(response);
    std::string        ok;
    int                messageCount;
    size_t             totalSize;
    ss >> ok >> messageCount >> totalSize;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3StatResponse>(messageCount, totalSize));
}

void Pop3Client::handleUidl(const std::string& response) {
    // 解析无参数的 UIDL 响应 (+OK <邮件编号> <邮件唯一id> ... <邮件编号> <邮件唯一id> .)
    std::istringstream ss(response);
    std::string        line;
    std::string        ok;
    // 用于保存解析后的编号和UID
    std::vector<Pop3UidlResponse::UidlItem> uids;
    // 逐行读取
    while (std::getline(ss, line)) {
        // 移除末尾可能的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // 跳过 +OK 行
        if (line == "+OK") {
            continue;
        }
        // 以 . 行结束
        if (line == ".") {
            break;
        }
        // 解析num和uid
        std::istringstream lineStream(line);
        int                num;
        std::string        uid;
        lineStream >> num >> uid;
        if (lineStream) {
            uids.emplace_back(num, uid);
        }
    }
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3UidlResponse>(uids));
}

void Pop3Client::handleUidlN(const std::string& response) {
    // 解析带参数的 UIDL 响应 (+OK <邮件编号> <邮件唯一id>)
    std::istringstream ss(response);
    std::string        ok;
    int                num;
    std::string        id;
    ss >> ok >> num >> id;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3UidlNResponse>(num, id));
}

void Pop3Client::handleList(const std::string& response) {
    // 解析无参数的 LIST 响应 (+OK <邮件编号> <邮件大小> ... <邮件编号> <邮件大小> .)
    std::istringstream ss(response);
    std::string        line;
    std::string        ok;
    // 用于保存解析后的编号和UID
    std::vector<Pop3ListResponse::ListItem> sizes;
    // 逐行读取
    while (std::getline(ss, line)) {
        // 移除末尾可能的 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        // 跳过 +OK 行
        if (line == "+OK") {
            continue;
        }
        // 以 . 行结束
        if (line == ".") {
            break;
        }
        // 解析num和uid
        std::stringstream lineStream(line);
        int               num;
        size_t            size;
        lineStream >> num >> size;
        if (lineStream) {
            sizes.emplace_back(num, size);
        }
    }
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3ListResponse>(sizes));
}

void Pop3Client::handleListN(const std::string& response) {
    // 解析带参数的 LIST 响应 (+OK <邮件编号> <邮件大小>)
    std::istringstream ss(response);
    std::string        ok;
    int                num;
    size_t             size;
    ss >> ok >> num >> size;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3ListNResponse>(num, size));
}

void Pop3Client::handleRetr(const std::string& response) {
    std::istringstream iss(response);
    std::string        line;
    std::string        okLine;

    std::getline(iss, okLine);
    if (okLine.rfind("+OK", 0) != 0) {
        return;  // 忽略非成功响应
    }

    std::ostringstream contentStream;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // 移除末尾的 \r
        }
        if (line == ".") {
            break;  // 结束标志
        }
        // POP3 为透明传输，若某行原本是以 "." 开头，服务器会变成 ".."
        // 所以客户端需要将 ".." 开头的行还原为 "."
        if (!line.empty() && line[0] == '.' && line.size() > 1 && line[1] == '.') {
            line.erase(0, 1);  // 移除多余的一个点
        }

        contentStream << line << "\r\n";  // 保持邮件格式
    }
    auto temp = contentStream.str();
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3RetrResponse>(contentStream.str()));
}

void Pop3Client::handleDele(const std::string& response) {
    std::istringstream iss(response);
    std::string        ok;
    std::string        message;
    iss >> ok >> message;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3DeleResponse>(message));
}

void Pop3Client::handleRset(const std::string& response) {
    std::istringstream iss(response);
    std::string        ok;
    std::string        message;
    iss >> ok >> message;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3RsetResponse>(message));
}

void Pop3Client::handleTop(const std::string& response) {
    std::istringstream iss(response);
    std::string        line;
    std::string        okLine;

    std::getline(iss, okLine);

    std::ostringstream headerStream;
    std::ostringstream bodyStream;
    bool               inBody = false;
    // 不断读取直到遇到.
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // 移除末尾的 \r
        }
        if (line == ".") {
            break;  // 结束标志
        }
        if (!inBody) {
            if (line.empty()) {
                inBody = true;  // 空行之后为正文预览
            } else {
                headerStream << line << "\r\n";
            }
        } else {
            bodyStream << line << "\r\n";
        }
    }
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3TopResponse>(headerStream.str(), bodyStream.str()));
}

void Pop3Client::handleNoop(const std::string& response) {
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3NoopResponse>());
}

void Pop3Client::handleCapa(const std::string& response) {
    std::istringstream iss(response);
    std::string        line;
    // 忽略第一行的+OK
    std::getline(iss, line);

    std::vector<std::string> capabilities;
    // 读取后续多行，直到遇到单独的 "."
    while (std::getline(iss, line)) {
        // 移除可能的 \r（来自 \r\n）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line == ".") {
            break;
        }
        capabilities.emplace_back(line);
    }
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3CapaResponse>(capabilities));
}

void Pop3Client::handleQuit(const std::string& response) {
    std::istringstream iss(response);
    std::string        ok;
    std::string        message;
    iss >> ok >> message;
    GET_AND_CALL_CALLBACK(std::make_shared<Pop3QuitResponse>(message));
    m_state = State::UPDATE;
    m_client.disconnect();
}

#undef GET_AND_CALL_CALLBACK

void Pop3Client::handleError(const std::string& error) {
    ZMUDUO_LOG_ERROR << "POP3 错误: " << error;
    m_client.disconnect();
}

#define PUSH_INTO_QUEUE(command, message, type)                                                    \
    do {                                                                                           \
        if (m_state == State::TRANSACTION) {                                                       \
            m_commands.emplace(command);                                                           \
            m_callbacks.emplace(wrapCallback<type>(std::move(callback)));                          \
            m_client.send(message);                                                                \
        }                                                                                          \
    } while (false)

void Pop3Client::stat(std::function<void(const Pop3StatResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::STAT, "STAT\r\n", Pop3StatResponse);
}

void Pop3Client::uidl(std::function<void(const Pop3UidlResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::UIDL, "UIDL\r\n", Pop3UidlResponse);
}

void Pop3Client::uidl(int num, std::function<void(const Pop3UidlNResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::UIDL_N, "UIDL " + std::to_string(num) + "\r\n", Pop3UidlNResponse);
}

void Pop3Client::list(std::function<void(const Pop3ListResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::LIST, "LIST\r\n", Pop3ListResponse);
}

void Pop3Client::list(int num, std::function<void(const Pop3ListNResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::LIST_N, "LIST " + std::to_string(num) + "\r\n", Pop3ListNResponse);
}
void Pop3Client::retr(int num, std::function<void(const Pop3RetrResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::RETR, "RETR " + std::to_string(num) + "\r\n", Pop3RetrResponse);
}

void Pop3Client::dele(int num, std::function<void(const Pop3DeleResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::DELE, "DELE " + std::to_string(num) + "\r\n", Pop3DeleResponse);
}

void Pop3Client::noop(std::function<void(const Pop3NoopResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::NOOP, "NOOP\r\n", Pop3NoopResponse);
}

void Pop3Client::rset(std::function<void(const Pop3RsetResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::RSET, "RSET\r\n", Pop3RsetResponse);
}

void Pop3Client::top(int num, int line, std::function<void(const Pop3TopResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::TOP,
                    "TOP " + std::to_string(num) + " " + std::to_string(line) + "\r\n",
                    Pop3TopResponse);
}
void Pop3Client::quit(std::function<void(const Pop3QuitResponse::Ptr&)> callback) {
    PUSH_INTO_QUEUE(Command::QUIT, "QUIT\r\n", Pop3QuitResponse);
}

template <typename T>
Pop3Client::CommandCallback Pop3Client::wrapCallback(std::function<void(std::shared_ptr<T>)> cb) {
    return [cb = std::move(cb)](Pop3Response::Ptr resp) {
        if (!resp) {
            cb(nullptr);
            return;
        }
        auto casted = std::dynamic_pointer_cast<T>(resp);
        if (casted) {
            cb(casted);
        } else {
            cb(nullptr);
        }
    };
}

#undef PUSH_INTO_QUEUE
}  // namespace zmuduo::net::email
