#include "webdav_server.h"
#include "zmuduo/base/logger.h"
#include <zmuduo/base/utils/string_util.h>

int main() {
    EventLoop    loop;
    auto         address = IPv4Address::Create("127.0.0.1", 8080);
    WebDavServer server(&loop, address, "/mnt/d/download", "/webdav", "admin", "123456");
    server.setThreadNum(4);
    // 对大文件进行range处理
    server.setMaxBodySize(30 * 1024 * 1024);
    server.start();
    loop.loop();
    return 0;
}