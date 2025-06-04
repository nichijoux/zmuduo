#pragma once
#include <filesystem>
#include <zmuduo/net/http/http_server.h>

using namespace zmuduo;
using namespace zmuduo::net;
using namespace zmuduo::net::http;

class WebDavServer : NoCopyable {
  public:
    WebDavServer(EventLoop*          loop,
                 const Address::Ptr& listenAddress,
                 const std::string&  sharePath,
                 const std::string&  prefixPath,
                 std::string         username = "",
                 std::string         password = "");

    void start() {
        m_server.start();
    }

    void setThreadNum(int num) {
        m_server.setThreadNum(num);
    }

    void setMaxBodySize(int maxChunkSize) {
        m_maxChunkSize = maxChunkSize;
    }

    void setUsername(std::string username) {
        m_username = std::move(username);
    }

    void setPassword(std::string password) {
        m_password = std::move(password);
    }

  private:
    enum class WebDavDepth { Zero, One, Infinity };

    bool checkBasicAuth(const HttpRequest& request);
    bool checkAuth(const HttpRequest& request, HttpResponse& response);

    static void handleOptions(const HttpRequest& request, HttpResponse& response);

    void handlePropfind(const HttpRequest& request, HttpResponse& response);
    void handleMkcol(const HttpRequest& request, HttpResponse& response);
    void handlePut(const HttpRequest& request, HttpResponse& response);
    void handleDelete(const HttpRequest& request, HttpResponse& response);
    void handleGet(const HttpRequest& request, HttpResponse& response);
    void handleHead(const HttpRequest& request, HttpResponse& response);
    void handleMove(const HttpRequest& request, HttpResponse& response);
    void handleCopy(const HttpRequest& request, HttpResponse& response);

    static void
    setDavError(HttpResponse& response, HttpStatus status, const std::string& message = "");

    std::filesystem::path getDestinationPath(const std::string& requestPath) const;

    static inline WebDavDepth parseDepthHeader(const HttpRequest& request) {
        std::string depth = request.getHeader("Depth");
        if (depth == "0") {
            return WebDavDepth::Zero;
        } else if (depth == "1") {
            return WebDavDepth::One;
        } else {
            return WebDavDepth::Infinity;  // 默认无限递归
        }
    }

  private:
    HttpServer  m_server;
    std::string m_sharePath;

    std::string m_prefixPath;
    std::string m_username;
    std::string m_password;

    int        m_maxChunkSize;
};