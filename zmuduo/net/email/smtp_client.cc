#include "zmuduo/net/email/smtp_client.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/base/utils/common_util.h"
#include "zmuduo/base/utils/hash_util.h"
#include "zmuduo/net/address.h"
#include "zmuduo/net/event_loop.h"
#include "zmuduo/net/tcp_connection.h"
#include <charconv>
#include <set>
#include <utility>

namespace zmuduo::net::email {
using namespace utils;

SmtpClient::SmtpClient(EventLoop* loop, const std::string& uri, std::string name)
    : SmtpClient(loop, CommonUtil::CheckNotNull(Uri::Create(uri)), std::move(name)) {}

SmtpClient::SmtpClient(zmuduo::net::EventLoop* loop, const zmuduo::net::Uri& uri, std::string name)
    : SmtpClient(loop, uri.createAddress(), std::move(name)) {
    assert(uri.getScheme() == "smtp");
}

SmtpClient::SmtpClient(EventLoop* loop, const Address::Ptr& hostAddress, std::string name) noexcept
    : m_client(loop, hostAddress, std::move(name)),
      m_state(State::DISCONNECT),
      m_successCallback(nullptr),
      m_failureCallback(nullptr) {
    m_client.setConnectionCallback(
        [this](const TcpConnectionPtr& connection) { onConnection(connection); });
    m_client.setMessageCallback(
        [this](const TcpConnectionPtr& connection, Buffer& buffer, const Timestamp& receiveTime) {
            onMessage(connection, buffer, receiveTime);
        });
}

void SmtpClient::send(const EMail::Ptr& email) {
    m_client.getEventLoop()->runInLoop([this, email] { sendInLoop(email); });
}

void SmtpClient::sendInLoop(const EMail::Ptr& email) {
    if (m_state != State::DISCONNECT) {
        ZMUDUO_LOG_ERROR << "SMTP client is busy";
        if (m_failureCallback) {
            m_failureCallback("SMTP client is busy");
        }
        return;
    }
    // 连接smtp服务器
    m_client.connect();
    std::string username = email->getFromEMailAddress();
    std::string password = email->getFromEMailPassword();
    auto        pos      = username.find('@');
    m_commands.clear();
    m_commands.emplace_back("HELO " + username.substr(pos + 1) + "\r\n");
    m_commands.emplace_back("AUTH LOGIN\r\n");
    // 用户名和密码
    m_commands.emplace_back(utils::HashUtil::Base64encode(username.substr(0, pos)) + "\r\n");
    m_commands.emplace_back(utils::HashUtil::Base64encode(password) + "\r\n");
    m_commands.emplace_back("MAIL FROM:<" + email->getFromEMailAddress() + ">\r\n");
    // 多个收件人
    std::set<std::string> targets;
#define INIT_RCPT(fun)                                                                             \
    for (const auto& x : email->fun()) {                                                           \
        targets.insert(x);                                                                         \
    }
    INIT_RCPT(getToEMailAddress)
    INIT_RCPT(getCcEMailAddress)
    INIT_RCPT(getBccEMailAddress)
#undef INIT_RCPT
    for (const auto& to : targets) {
        m_commands.emplace_back("RCPT TO:<" + to + ">\r\n");
    }
    // 传输数据
    m_commands.emplace_back("DATA\r\n");
    auto&             entities = email->getEntities();
    std::stringstream buffer;
    buffer << "From: <" << email->getFromEMailAddress() << ">\r\nTo: ";
#define MULTI_ADDRESS_NEED_TO_SEND(func)                                                           \
    do {                                                                                           \
        const auto& v = email->func();                                                             \
        for (size_t i = 0; i < v.size(); ++i) {                                                    \
            if (i) {                                                                               \
                buffer << ",";                                                                     \
            }                                                                                      \
            buffer << "<" << v[i] << ">";                                                          \
        }                                                                                          \
        if (!v.empty()) {                                                                          \
            buffer << "\r\n";                                                                      \
        }                                                                                          \
    } while (0)
    MULTI_ADDRESS_NEED_TO_SEND(getToEMailAddress);
    if (!email->getCcEMailAddress().empty()) {
        buffer << "Cc: ";
        MULTI_ADDRESS_NEED_TO_SEND(getCcEMailAddress);
    }
#undef MULTI_ADDRESS_NEED_TO_SEND
    buffer << "Subject: " << email->getTitle() << "\r\n";
    std::string boundary;
    if (!entities.empty()) {
        boundary = utils::HashUtil::RandomString(16);
        buffer << "Content-Type: multipart/mixed;boundary=" << boundary << "\r\n";
    }
    buffer << "MIME-Version: 1.0\r\n";
    if (!boundary.empty()) {
        buffer << "\r\n--" << boundary << "\r\n";
    }
    buffer << "Content-Type: text/html;charset=\"utf-8\"\r\n"
           << "\r\n"
           << email->getBody() << "\r\n";
    for (auto& entity : entities) {
        buffer << "\r\n--" << boundary << "\r\n";
        buffer << entity->toString();
    }
    if (!boundary.empty()) {
        buffer << "\r\n--" << boundary << "--\r\n";
    }
    // data结束的标志
    buffer << "\r\n.\r\n";
    m_commands.emplace_back(buffer.str());
    m_commands.emplace_back("QUIT\r\n");
}

void SmtpClient::onConnection(const TcpConnectionPtr& connection) {
    if (connection->isConnected()) {
        m_state = State::CONNECTED;
    } else {
        m_state = State::DISCONNECT;
        if (!m_commands.empty()) {
            // Connection closed before completing
            handleError("Connection closed unexpectedly");
        }
    }
}

void SmtpClient::onMessage(const TcpConnectionPtr& connection,
                           Buffer&                 buffer,
                           const Timestamp&        receiveTime) {
    // 如果有可读取字节则不断循环
    while (buffer.getReadableBytes()) {
        const char* crlf = buffer.findCRLF();
        if (crlf) {
            std::string response(buffer.peek(), crlf);
            buffer.retrieveUntil(crlf + strlen(Buffer::S_CRLF));
            handleResponse(response);
        } else {
            break;
        }
    }
}

void SmtpClient::handleResponse(const std::string& response) {
    ZMUDUO_LOG_DEBUG << "SMTP response: " << response;
    if (response.empty()) {
        return;
    }
    // 获取smtp服务器返回代码
    int code;
    std::from_chars(response.c_str(), response.c_str() + 3, code);
    bool isLastResponse = (code >= 200 && code < 400);
    // 是否发送完毕
    if (!isLastResponse) {
        return;
    }
#define DO_WITH_ERROR(targetCode, errorMessage)                                                      \
    do {                                                                                           \
        if (code == targetCode) {                                                                  \
            m_state = static_cast<State>(static_cast<int>(m_state) + 1);                           \
            sendNextCommand();                                                                     \
        } else {                                                                                   \
            handleError(errorMessage);                                                             \
        }                                                                                          \
    } while (false)

    switch (m_state) {
        case State::CONNECTED: DO_WITH_ERROR(220, "Unexpected response after connection"); break;
        case State::HELO_SENT: DO_WITH_ERROR(250, "HELO failed"); break;
        case State::AUTH_SENT: DO_WITH_ERROR(334, "AUTH LOGIN failed"); break;
        case State::USERNAME_SENT: DO_WITH_ERROR(334, "Username not accepted"); break;
        case State::PASSWORD_SENT: DO_WITH_ERROR(235, "Authentication failed"); break;
        case State::MAIL_FROM_SENT: DO_WITH_ERROR(250, "MAIL FROM failed"); break;
        case State::RCPT_TO_SENT:
            // 对于每个RCPT TO命令，服务器都会返回250
            if (code == 250) {
                bool moreRcpt = false;
                // 检查是否还有待处理的RCPT TO命令
                for (const auto& cmd : m_commands) {
                    if (cmd.find("RCPT TO:") == 0) {
                        moreRcpt = true;
                        break;
                    }
                }
                if (!moreRcpt) {
                    // 如果rcpt已经发送完毕,则转移状态
                    m_state = State::DATA_SENT;
                }
                sendNextCommand();
            } else {
                handleError("RCPT TO failed for one recipient");
            }
            break;
        case State::DATA_SENT: DO_WITH_ERROR(354, "send DATA failed"); break;
        case State::QUIT_SENT: DO_WITH_ERROR(250, "send QUIT failed"); break;
        case State::FINISHED: sendNextCommand(); break;
        case State::DISCONNECT: m_client.disconnect(); break;
    }
#undef DO_WITH_ERROR
}

void SmtpClient::sendNextCommand() {
    // 如果命令全部发送完毕，则进入完成状态
    if (m_commands.empty()) {
        if (m_state == State::FINISHED) {
            if (m_successCallback) {
                m_successCallback();
            }
            m_client.disconnect();
        }
        return;
    }
    // 否则，则获取一个命令并发送
    const auto cmd = m_commands.front();
    m_commands.pop_front();
    sendCommand(cmd);
}

void SmtpClient::sendCommand(const std::string& cmd) {
    if (m_state != State::DISCONNECT) {
        ZMUDUO_LOG_DEBUG << "SMTP command: " << cmd;
        // 发送命令
        m_client.send(cmd);
    }
}

void SmtpClient::handleError(const std::string& error) {
    ZMUDUO_LOG_ERROR << "SMTP error: " << error;
    m_commands.clear();
    if (m_failureCallback) {
        m_failureCallback(error);
    }
    m_client.disconnect();
}
}  // namespace zmuduo::net::email