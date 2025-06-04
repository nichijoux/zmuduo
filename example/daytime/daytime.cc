#include "daytime.h"

#include "zmuduo/base/logger.h"
#include "zmuduo/net/event_loop.h"

DaytimeServer::DaytimeServer(EventLoop* loop, const Address::Ptr& listenAddr)
    : server_(loop, listenAddr, "DaytimeServer") {
    server_.setConnectionCallback(
        [this](auto&& PH1) { onConnection(std::forward<decltype(PH1)>(PH1)); });
    server_.setMessageCallback([this](auto&& PH1, auto&& PH2, auto&& PH3) {
        onMessage(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                  std::forward<decltype(PH3)>(PH3));
    });
}

void DaytimeServer::start() {
    server_.start();
}

void DaytimeServer::onConnection(const TcpConnectionPtr& conn) {
    ZMUDUO_LOG_FMT_INFO(
        "DaytimeServer - %s -> %s is %s", conn->getPeerAddress()->toString().c_str(),
        conn->getLocalAddress()->toString().c_str(), (conn->isConnected() ? "UP" : "DOWN"));
    if (conn->isConnected()) {
        conn->send(Timestamp::Now().toString() + "\n");
        conn->shutdown();
    }
}

void DaytimeServer::onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp& time) {
    std::string msg(buf.retrieveAllAsString());
    ZMUDUO_LOG_FMT_INFO("%s discards %zu bytes received at %s", conn->getName().c_str(), msg.size(),
                        time.toString().c_str());
}
