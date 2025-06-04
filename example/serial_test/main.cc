#include <iomanip>
#include <zmuduo/base/logger.h>
#include <zmuduo/net/serial_port.h>

using namespace zmuduo::thread;
using namespace zmuduo::net;

std::string toHex(const char* str, size_t len) {
    std::ostringstream oss;
    for (size_t i = 0; i < len; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)str[i] << " ";
    }
    return oss.str();
}

std::string toHex(const std::string& s) {
    return toHex(s.data(), s.size());
}

int main() {
    EventLoop  loop;
    SerialPort serialPort(&loop, "/dev/pts/3");
    serialPort.setOpenedCallback([&serialPort](bool opened) {
        if (opened) {
            ZMUDUO_LOG_IMPORTANT << "打开串口成功";
            serialPort.send("hello world");
        }
    });
    serialPort.setMessageCallback([](SerialPort& port, Buffer& in) {
        auto s = in.retrieveAllAsString();
        ZMUDUO_LOG_IMPORTANT << s << "|" << toHex(s);
        port.send("you said: " + s);
    });
    serialPort.open();
    loop.loop();
}