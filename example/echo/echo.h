#pragma once

#include "zmuduo/net/tcp_server.h"

using namespace zmuduo;
using namespace zmuduo::net;

// RFC 862
class EchoServer {
  public:
    EchoServer(EventLoop* loop, const Address::Ptr& listenAddr);

    void start();  // calls server_.start();

  private:
    void onConnection(const TcpConnectionPtr& conn);

    void onMessage(const TcpConnectionPtr& conn, Buffer& buf, const Timestamp& time);

    TcpServer server_;
};
