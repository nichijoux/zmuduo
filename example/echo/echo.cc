#include "echo.h"

#include "zmuduo/base/logger.h"

EchoServer::EchoServer(EventLoop* loop, const Address::Ptr& listenAddr)
    : server_(loop, listenAddr, "EchoServer") {
    server_.setConnectionCallback(
        [this](auto&& PH1) { onConnection(std::forward<decltype(PH1)>(PH1)); });
    server_.setMessageCallback([this](auto&& PH1, auto&& PH2, auto&& PH3) {
        onMessage(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2),
                  std::forward<decltype(PH3)>(PH3));
    });
}

void EchoServer::start() {
    server_.start();
}

void EchoServer::onConnection(const TcpConnectionPtr& conn) {
    ZMUDUO_LOG_FMT_INFO("EchoServer - %s -> %s is %s", conn->getPeerAddress()->toString().c_str(),
                        conn->getLocalAddress()->toString().c_str(),
                        (conn->isConnected() ? "UP" : "DOWN"));
}

void EchoServer::onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp& time) {
    std::string msg(buf.retrieveAllAsString());
    ZMUDUO_LOG_FMT_INFO("%s echo %zu bytes, data received at %s", conn->getName().c_str(),
                        msg.size(), time.toString().c_str());
    conn->send("You said:" + msg + "\n");
}
