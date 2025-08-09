#include <zmuduo/net/tcp_client.h>

#include <zmuduo/base/logger.h>
#include <zmuduo/base/thread.h>
#include <zmuduo/base/types.h>
#include <zmuduo/base/utils/system_util.h>
#include <zmuduo/net/buffer.h>
#include <zmuduo/net/event_loop.h>
#include <zmuduo/net/event_loop_thread_pool.h>
#include <zmuduo/net/tcp_connection.h>

#include <utility>

#include <cstdio>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::thread;
using namespace zmuduo::utils::system_util;

class Client;

class Session : NoCopyable {
  public:
    Session(EventLoop* loop, const Address::Ptr& serverAddr, const std::string& name, Client* owner)
        : client_(loop, serverAddr, name),
          owner_(owner),
          bytesRead_(0),
          bytesWritten_(0),
          messagesRead_(0) {
        client_.setConnectionCallback(
            [this](auto&& PH1) { onConnection(std::forward<decltype(PH1)>(PH1)); });
        client_.setMessageCallback([this](auto&& PH1, auto&& PH2, auto&& PH3) {
            onMessage(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                      std::forward<decltype(PH3)>(PH3));
        });
    }

    void start() {
        client_.connect();
    }

    void stop() {
        client_.disconnect();
    }

    int64_t bytesRead() const {
        return bytesRead_;
    }

    int64_t messagesRead() const {
        return messagesRead_;
    }

  private:
    void onConnection(const TcpConnectionPtr& conn);

    void onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp&) {
        ++messagesRead_;
        bytesRead_ += static_cast<decltype(bytesRead_)>(buf.getReadableBytes());
        bytesWritten_ += static_cast<decltype(bytesWritten_)>(buf.getReadableBytes());
        conn->send(buf);
    }

    TcpClient client_;
    Client*   owner_;
    int64_t   bytesRead_;
    int64_t   bytesWritten_;
    int64_t   messagesRead_;
};

class Client : NoCopyable {
  public:
    Client(EventLoop*          loop,
           const Address::Ptr& serverAddr,
           int                 blockSize,
           int                 sessionCount,
           int                 timeout,
           int                 threadCount)
        : loop_(loop),
          threadPool_(loop, "pingpong-client"),
          sessionCount_(sessionCount),
          timeout_(timeout) {
        loop->runAfter(timeout, [this] { handleTimeout(); });
        if (threadCount > 1) {
            threadPool_.setThreadNum(threadCount);
        }
        threadPool_.start();

        for (int i = 0; i < blockSize; ++i) {
            message_.push_back(static_cast<char>(i % 128));
        }

        for (int i = 0; i < sessionCount; ++i) {
            char buf[32];
            snprintf(buf, sizeof buf, "C%05d", i);
            auto* session = new Session(threadPool_.getNextLoop(), serverAddr, buf, this);
            session->start();
            sessions_.emplace_back(session);
        }
    }

    const std::string& message() const {
        return message_;
    }

    void onConnect() {
        if (numConnected_++ == sessionCount_) {
            ZMUDUO_LOG_FMT_WARNING("all connected");
        }
    }

    void onDisconnect(const TcpConnectionPtr& conn) {
        if (numConnected_++ == 0) {
            ZMUDUO_LOG_FMT_WARNING("all disconnected");

            int64_t totalBytesRead    = 0;
            int64_t totalMessagesRead = 0;
            for (const auto& session : sessions_) {
                totalBytesRead += session->bytesRead();
                totalMessagesRead += session->messagesRead();
            }
            ZMUDUO_LOG_FMT_WARNING("%ld total bytes read", totalBytesRead);
            ZMUDUO_LOG_FMT_WARNING("%ld total messages read", totalMessagesRead);
            ZMUDUO_LOG_FMT_WARNING("%f average message size",
                                   static_cast<double>(totalBytesRead) /
                                       static_cast<double>(totalMessagesRead));
            ZMUDUO_LOG_FMT_WARNING("%f MiB/s throughput",
                                   static_cast<double>(totalBytesRead) / (timeout_ * 1024 * 1024));
            conn->getEventLoop()->queueInLoop([this] { quit(); });
        }
    }

  private:
    void quit() {
        loop_->queueInLoop([this] { loop_->quit(); });
    }

    void handleTimeout() {
        ZMUDUO_LOG_FMT_WARNING("stop");
        for (auto& session : sessions_) {
            session->stop();
        }
    }

    EventLoop*                            loop_;
    EventLoopThreadPool                   threadPool_;
    int                                   sessionCount_;
    int                                   timeout_;
    std::vector<std::unique_ptr<Session>> sessions_;
    std::string                           message_;
    std::atomic<int64_t>                  numConnected_{};
};

void Session::onConnection(const TcpConnectionPtr& conn) {
    if (conn->isConnected()) {
        conn->setTcpNoDelay(true);
        conn->send(owner_->message());
        owner_->onConnect();
    } else {
        owner_->onDisconnect(conn);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: client <host_ip> <port> <threads> <blocksize> ");
        fprintf(stderr, "<sessions> <time>\n");
    } else {
        ZMUDUO_LOG_FMT_INFO("pid =  %d, tid = %d", GetPid(), GetTid());

        const char* ip           = argv[1];
        auto        port         = lexical_cast<uint16_t>(argv[2]);
        int         threadCount  = lexical_cast<int>(argv[3]);
        int         blockSize    = lexical_cast<int>(argv[4]);
        int         sessionCount = lexical_cast<int>(argv[5]);
        int         timeout      = lexical_cast<int>(argv[6]);

        EventLoop loop;
        auto      serverAddr = IPv4Address::Create(ip, port);

        Client client(&loop, serverAddr, blockSize, sessionCount, timeout, threadCount);
        loop.loop();
    }
}
