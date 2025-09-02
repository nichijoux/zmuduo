#include "zmuduo/net/serial_port.h"
#include "socket_options.h"
#include "zmuduo/base/logger.h"
#include "zmuduo/net/channel.h"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <termios.h>
#include <unistd.h>
#include <utility>

namespace zmuduo::net {
SerialPort::SerialPort(EventLoop* loop, std::string portName, const SerialConfig& config)
    : m_eventLoop(loop),
      m_portName(std::move(portName)),
      m_config(config) {}

SerialPort::~SerialPort() {
    close();
}

void SerialPort::open() {
    assert(!m_opened);

    // O_RDWR: 读写模式
    // O_NOCTTY: 不将此设备作为控制终端
    // O_NONBLOCK: 非阻塞模式打开
    m_fd = ::open(m_portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (m_fd == -1) {
        ZMUDUO_LOG_ERROR << "Cannot open port " << m_portName << ": " << strerror(errno);
        return;
    }

    // 应用设置
    applyConfig();
    // 创建并启动channel
    m_channel = std::make_unique<Channel>(m_eventLoop, m_fd);
    m_channel->setReadCallback([this](auto) { handleRead(); });
    m_channel->setWriteCallback([this] { handleWrite(); });
    m_channel->setErrorCallback([this] { handleError(); });
    m_channel->enableReading();
    // 设置串口状态为已经打开
    m_opened = true;
    if (m_openedCallback) {
        m_openedCallback(m_opened);
    }
}

void SerialPort::close() {
    if (m_opened) {
        handleClose();
    }
}

void SerialPort::send(const char* data, size_t length) {
    if (m_opened) {
        // 如果已经成功连接
        m_eventLoop->runInLoop([this, data, length] {
            // 捕获 message并发送
            sendInLoop(data, length);
        });
    }
}

void SerialPort::setConfig(const SerialConfig& config) {
    assert(!m_opened);
    m_config = config;
}

void SerialPort::applyConfig() const {
    termios tty{};
    bzero(&tty, sizeof(tty)); // 初始化为 0

    if (tcgetattr(m_fd, &tty) != 0) {
        ZMUDUO_LOG_ERROR << "Error from tcgetattr: " << strerror(errno);
        return;
    }

    // --- 波特率 ---
    cfsetospeed(&tty, m_config.baudRate);
    cfsetispeed(&tty, m_config.baudRate);

    // --- 控制标志 (c_cflag) ---
    tty.c_cflag &= ~CSIZE; // 清除数据位设置
    switch (m_config.dataBits) {
        case SerialConfig::DB_5: tty.c_cflag |= CS5;
            break;
        case SerialConfig::DB_6: tty.c_cflag |= CS6;
            break;
        case SerialConfig::DB_7: tty.c_cflag |= CS7;
            break;
        case SerialConfig::DB_8:
        default: tty.c_cflag |= CS8;
            break; // 默认为 8 位
    }
    // 校验设置
    switch (m_config.parity) {
        case SerialConfig::NONE: tty.c_cflag &= ~PARENB; // 关闭校验
            tty.c_iflag &= ~INPCK;                       // 关闭输入校验
            break;
        case SerialConfig::ODD: tty.c_cflag |= PARENB; // 开启校验
            tty.c_cflag |= PARODD;                     // 奇校验
            tty.c_iflag |= INPCK;                      // 开启输入校验
            break;
        case SerialConfig::EVEN: tty.c_cflag |= PARENB; // 开启校验
            tty.c_cflag &= ~PARODD;                     // 偶校验
            tty.c_iflag |= INPCK;                       // 开启输入校验
            break;
    }
    // 停止位设置
    switch (m_config.stopBits) {
        case SerialConfig::ONE: tty.c_cflag &= ~CSTOPB;
            break; // 1 停止位
        case SerialConfig::TWO: tty.c_cflag |= CSTOPB;
            break; // 2 停止位
    }

    tty.c_cflag |= CREAD | CLOCAL; // 开启接收器, 忽略调制解调器控制线

    // --- 本地标志 (c_lflag) ---
    // 设置为原始模式 (Raw Mode)
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

    // --- 输入标志 (c_iflag) ---
    // 关闭软件流控和特殊字符处理
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // --- 输出标志 (c_oflag) ---
    // 关闭所有输出处理 (原始模式)
    tty.c_oflag &= ~OPOST;

    // --- 控制字符 (c_cc) ---
    // VMIN: 最小读取字节数。VTIME: 超时时间 (0.1秒为单位)。
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = static_cast<cc_t>(0.01);

    // --- 应用设置 ---
    tcflush(m_fd, TCIOFLUSH); // 清空输入输出缓冲区
    if (tcsetattr(m_fd, TCSANOW, &tty) != 0) {
        // TCSANOW: 立即生效
        ZMUDUO_LOG_ERROR << "Error from tcsetattr: " << strerror(errno);
    }
}

void SerialPort::handleRead() {
    int savedErrno;
    // 收到数据
    if (const ssize_t n = m_inputBuffer.readFD(m_fd, &savedErrno); n > 0) {
        if (m_messageCallback) {
            m_messageCallback(*this, m_inputBuffer);
        }
    } else if (n == 0) {
        handleClose();
    } else {
        errno = savedErrno;
        if (errno != EAGAIN) {
            ZMUDUO_LOG_ERROR << strerror(errno);
            handleError();
        }
    }
}

void SerialPort::handleWrite() {
    m_eventLoop->assertInLoopThread();
    if (m_channel->isWriting()) {
        int savedErrno;
        if (const ssize_t n = m_outputBuffer.writeFD(m_channel->getFD(), &savedErrno); n > 0) {
            m_outputBuffer.retrieve(n);
            if (m_outputBuffer.getReadableBytes() == 0) {
                // 发送完成
                m_channel->disableWriting();
            }
        } else if (n == 0) {
            handleClose();
        } else {
            errno = savedErrno;
            ZMUDUO_LOG_ERROR << "handleWrite,errno: " << strerror(errno);
            handleError();
        }
    } else {
        ZMUDUO_LOG_FMT_ERROR("Connection fd =%d is down, but still in writing", m_channel->getFD());
    }
}

void SerialPort::handleClose() {
    ::close(m_fd);
    m_fd     = -1;
    m_opened = false;
    // 移除channel
    m_channel->disableAll();
    m_channel->remove();
    // 关闭的回调
    if (m_openedCallback) {
        m_openedCallback(m_opened);
    }
}

void SerialPort::handleError() const {
    // 获取错误码
    const int savedErrno = sockets::getSocketError(m_channel->getFD());
    ZMUDUO_LOG_ERROR << "SO_ERROR because " << strerror(savedErrno);
}

void SerialPort::sendInLoop(const void* data, const size_t length) {
    if (length == 0) {
        return;
    }
    m_eventLoop->assertInLoopThread();
    ssize_t nwrote     = 0;
    size_t  remaining  = length;
    bool    faultErrno = false;
    if (!m_opened) {
        ZMUDUO_LOG_FMT_WARNING("closed, give up writing");
        return;
    }
    if (!m_channel->isWriting() && m_outputBuffer.getReadableBytes() == 0) {
        // channel第一次开始写数据且缓冲区没有待发送的数据
        nwrote = ::write(m_channel->getFD(), data, length);
        if (nwrote >= 0) {
            // 更新剩余待发送的数据
            remaining = length - nwrote;
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                ZMUDUO_LOG_ERROR << "sendInLoop";
                if (errno == EPIPE || errno == ECONNRESET) {
                    faultErrno = true;
                }
            }
        }
    }

    assert(remaining <= length);
    if (!faultErrno && remaining > 0) {
        // 没有将message全部发送出去,需要将数据保存到缓冲区中,并给channel注册EPOLLOUT事件
        // poller发现缓冲区有空间会通知相应channel调用writeCallback方法,也就是调用handleWrite方法

        // 添加没有发送的数据到缓冲区
        m_outputBuffer.write(static_cast<const char*>(data) + nwrote, remaining);
        // 如果对写事件不感兴趣则注册写事件
        if (!m_channel->isWriting()) {
            m_channel->enableWriting();
        }
    }
}
} // namespace zmuduo::net