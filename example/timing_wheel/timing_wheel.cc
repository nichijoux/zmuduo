#include <zmuduo/net/tcp_server.h>
#include <zmuduo/net/timer_id.h>

#include <list>
#include <unordered_set>
#include <utility>
#include <zmuduo/base/logger.h>

namespace timing_wheel {
using namespace zmuduo::net;

using WeakTcpConnectionPtr = std::weak_ptr<TcpConnection>;

struct Entry {
    explicit Entry(WeakTcpConnectionPtr ptr) : m_weakPtr(std::move(ptr)) {}

    ~Entry() {
        auto strongPtr = m_weakPtr.lock();
        if (strongPtr) {
            strongPtr->forceClose();
        }
    }

    WeakTcpConnectionPtr m_weakPtr;
};

using EntryPtr     = std::shared_ptr<Entry>;
using WeakEntryPtr = std::weak_ptr<Entry>;
using Bucket       = std::unordered_set<EntryPtr>;

class WeakConnectionList {
  public:
    explicit WeakConnectionList(int maxSize) : m_maxSize(maxSize), m_queue(maxSize), m_timerId() {}

    int getMaxSize() const {
        return m_maxSize;
    }

    void push_back(Bucket bucket) {
        m_queue.emplace_back(std::move(bucket));
        m_queue.pop_front();
    }

    Bucket& back() {
        return m_queue.back();
    }

  private:
    int               m_maxSize;
    std::list<Bucket> m_queue;
    TimerId           m_timerId;
};

class TimingWheelTcpServer {
  public:
    TimingWheelTcpServer(EventLoop*          loop,
                         const Address::Ptr& listenAddress,
                         const std::string&  name,
                         int                 maxConnectionTime,
                         bool                reusePort = false)
        : m_server(loop, listenAddress, name, reusePort),
          m_timingQueue(std::make_shared<WeakConnectionList>(maxConnectionTime)) {
        m_server.setConnectionCallback(
            [this](const TcpConnectionPtr& connection) { connectionTimingWheel(connection); });
        m_server.setMessageCallback([this](const TcpConnectionPtr& connection, Buffer& buffer,
                                           const zmuduo::Timestamp& timestamp) {
            messageTimingWheel(connection, buffer, timestamp);
        });
        loop->runEvery(1.0, [this]() { m_timingQueue->push_back(Bucket()); });
    }

    void setConnectionCallback(ConnectionCallback cb) {
        m_connectionCallback = std::move(cb);
    }

    void setMessageCallback(MessageCallback cb) {
        m_messageCallback = std::move(cb);
    }

    void start() {
        m_server.start();
    }

  private:
    void connectionTimingWheel(const TcpConnectionPtr& connection) {
        if (connection->isConnected()) {
            EntryPtr entry = std::make_shared<Entry>(connection);
            m_timingQueue->back().insert(entry);
            WeakEntryPtr weakEntryPtr(entry);
            connection->setContext(weakEntryPtr);
        }
        m_connectionCallback(connection);
    }

    void messageTimingWheel(const TcpConnectionPtr&  connection,
                            Buffer&                  buffer,
                            const zmuduo::Timestamp& timestamp) {
        // 更新位置
        auto     weakEntryPtr = std::any_cast<WeakEntryPtr>(connection->getContext());
        EntryPtr entry(weakEntryPtr.lock());
        if (entry) {
            m_timingQueue->back().insert(entry);
        }
        m_messageCallback(connection, buffer, timestamp);
    }

    std::shared_ptr<WeakConnectionList> m_timingQueue;
    TcpServer                           m_server;
    ConnectionCallback                  m_connectionCallback;
    MessageCallback                     m_messageCallback;
};

}  // namespace timing_wheel

using namespace zmuduo;
using namespace zmuduo::net;
using namespace timing_wheel;

int main() {
    EventLoop            loop;
    auto                 listenAddress = IPv4Address::Create("127.0.0.1", 8000);
    TimingWheelTcpServer server(&loop, listenAddress, "TimingWheelServer", 10);
    server.setConnectionCallback(defaultConnectionCallback);
    server.setMessageCallback([](const TcpConnectionPtr&, Buffer& buffer, const Timestamp&) {
        std::string s = buffer.retrieveAllAsString();
        ZMUDUO_LOG_FMT_INFO("接收到数据 %s", s.c_str());
    });
    server.start();

    loop.loop();
    return 0;
}