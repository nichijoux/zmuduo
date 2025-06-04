// Copyright (c) nichijoux (BSD 3-Clause)

#ifndef ZMUDUO_NET_SERIAL_PORT_H
#define ZMUDUO_NET_SERIAL_PORT_H

#include "zmuduo/net/buffer.h"
#include "zmuduo/net/event_loop.h"
#include <termios.h>
#include <vector>

namespace zmuduo::net {
/**
 * @struct SerialConfig
 * @brief 串口配置结构体，定义波特率、数据位、停止位和校验位。
 *
 * 用于配置 POSIX 串口参数，基于 termios 标准，支持常见波特率和通信设置。
 *
 * @note 波特率值依赖 termios.h 中的 Bxxx 常量，部分高波特率可能不受某些系统支持。
 */
struct SerialConfig {
    /**
     * @brief 波特率枚举
     */
    enum BaudRate {
        B_0       = B0,
        B_50      = B50,
        B_75      = B75,
        B_110     = B110,
        B_134     = B134,
        B_150     = B150,
        B_200     = B200,
        B_300     = B300,
        B_600     = B600,
        B_1200    = B1200,
        B_1800    = B1800,
        B_2400    = B2400,
        B_4800    = B4800,
        B_9600    = B9600,
        B_19200   = B19200,
        B_38400   = B38400,
        B_57600   = B57600,
        B_115200  = B115200,
        B_230400  = B230400,
        B_460800  = B460800,
        B_500000  = B500000,
        B_576000  = B576000,
        B_921600  = B921600,
        B_1000000 = B1000000,
        B_1152000 = B1152000,
        B_1500000 = B1500000,
        B_2000000 = B2000000,
        B_2500000 = B2500000,
        B_3000000 = B3000000,
        B_3500000 = B3500000,
        B_4000000 = B4000000,
    };

    /**
     * @brief 停止位枚举。
     */
    enum StopBits { ONE, TWO };

    /**
     * @brief 校验位枚举。
     */
    enum Parity { NONE, ODD, EVEN };

    /**
     * @brief 数据位枚举。
     */
    enum DataBits { DB_5 = 5, DB_6 = 6, DB_7 = 7, DB_8 = 8 };

    BaudRate baudRate = B_115200;  ///< 波特率，默认 115200
    StopBits stopBits = ONE;       ///< 停止位，默认 1
    Parity   parity   = NONE;      ///< 校验位，默认无校验
    DataBits dataBits = DB_8;      ///< 数据位，默认 8
};

/**
 * @class SerialPort
 * @brief 串口通信类，用于管理 POSIX 串口设备的异步读写。
 *
 * SerialPort 支持配置串口参数、异步数据读写和状态管理，基于 EventLoop 和 Channel 实现非阻塞通信。
 * 使用原始模式（Raw Mode）禁用行缓冲和特殊字符处理，适合嵌入式设备和传感器通信。
 *
 * @note SerialPort 不可拷贝（继承自 NoCopyable），通过 EventLoop 确保线程安全。
 * @note 回调函数需线程安全，缓冲区大小受系统限制。
 *
 * @example
 *
 * @code
 * EventLoop loop;
 * SerialConfig config;
 * config.baudRate = SerialConfig::B_9600;
 * SerialPort port(&loop, "/dev/ttyS0", config);
 * port.setMessageCallback([](Buffer& in, Buffer& out) {
 *     std::string data = in.retrieveAllAsString();
 *     std::cout << "Received: " << data << std::endl;
 *     out.append("ACK\r\n");
 * });
 * port.setOpenedCallback([](bool opened) {
 *     std::cout << "Serial port " << (opened ? "opened" : "closed") << std::endl;
 * });
 * port.open();
 * port.send("Hello\r\n", 7);
 * loop.loop();
 * @endcode
 */
class SerialPort : NoCopyable {
  public:
    /**
     * @typedef std::function&lt;void(bool opened)&gt;
     * @brief 串口打开/关闭回调
     * @param[in] opened 串口是否打开
     */
    using OpenedCallback = std::function<void(bool opened)>;
    /**
     * @typedef std::function&lt;void(SerialPort& serialPort, Buffer& inBuffer)&gt;
     * @brief 数据接收回调
     * @param[in,out] serialPort 串口通信类
     * @param[in,out] inBuffer 输入缓冲区
     */
    using MessageCallback = std::function<void(SerialPort& serialPort, Buffer& inBuffer)>;

  public:
    /**
     * @brief 构造函数。
     * @param[in,out] loop 所属事件循环。
     * @param[in] portName 串口设备路径（如 "/dev/ttyS0"）。
     * @param[in] config 串口配置，有默认值
     */
    SerialPort(EventLoop* loop, std::string portName, const SerialConfig& config = {});

    /**
     * @brief 析构函数。
     * @note 确保关闭串口并释放资源。
     */
    ~SerialPort();

    /**
     * @brief 打开串口。
     * @note 以非阻塞模式打开设备，应用配置并启用读事件。
     */
    void open();

    /**
     * @brief 关闭串口。
     * @note 关闭设备文件描述符，禁用事件并触发关闭回调。
     */
    void close();

    /**
     * @brief 检查串口是否打开。
     * @retval true 串口已打开。
     * @retval false 串口未打开。
     */
    bool isOpen() const {
        return m_opened;
    }

    /**
     * @brief 设置串口配置。
     * @param[in] config 新配置。
     * @note 必须在串口关闭时调用，打开后需重新打开串口以应用。
     */
    void setConfig(const SerialConfig& config);

    /**
     * @brief 获取当前串口配置。
     * @return 当前的 SerialConfig 实例。
     */
    SerialConfig getConfig() const {
        return m_config;
    }

    /**
     * @brief 发送数据。
     * @param[in] data 数据缓冲区。
     * @param[in] length 数据长度。
     * @note 异步发送，数据追加到输出缓冲区。
     */
    void send(const char* data, size_t length);

    /**
     * @brief 发送字符串数据。
     * @param[in] data 字符串数据。
     * @note 异步发送，等同于 send(data.c_str(), data.size())。
     */
    void send(const std::string& data) {
        send(data.c_str(), data.size());
    }

    /**
     * @brief 设置串口打开/关闭回调。
     * @param[in] callback 回调函数。
     */
    void setOpenedCallback(OpenedCallback callback) {
        m_openedCallback = std::move(callback);
    }

    /**
     * @brief 设置数据接收回调。
     * @param[in] callback 回调函数，接收输入和输出缓冲区。
     */
    void setMessageCallback(MessageCallback callback) {
        m_messageCallback = std::move(callback);
    }

  private:
    /**
     * @brief 应用串口配置。
     * @note 配置 termios 参数（波特率、数据位、停止位、校验位），设置原始模式。
     */
    void applyConfig() const;

    /**
     * @brief 处理读事件。
     * @note 从串口读取数据到输入缓冲区，触发消息回调。
     */
    void handleRead();

    /**
     * @brief 处理写事件。
     * @note 将输出缓冲区数据写入串口，完成后禁用写事件。
     */
    void handleWrite();

    /**
     * @brief 处理关闭事件。
     * @note 关闭文件描述符，禁用事件并触发关闭回调。
     */
    void handleClose();

    /**
     * @brief 处理错误事件。
     * @note 记录错误日志（SO_ERROR）。
     */
    void handleError();

    /**
     * @brief 在事件循环中发送数据。
     * @param[in] data 数据缓冲区。
     * @param[in] length 数据长度。
     * @note 直接写入或追加到输出缓冲区，启用写事件。
     */
    void sendInLoop(const void* data, size_t length);

  private:
    EventLoop*               m_eventLoop;        ///< 事件循环
    int                      m_fd;               ///< 串口文件描述符
    std::string              m_portName;         ///< 串口设备路径
    SerialConfig             m_config;           ///< 串口配置
    bool                     m_opened;           ///< 串口打开状态
    std::unique_ptr<Channel> m_channel;          ///< 事件通道
    Buffer                   m_inputBuffer;      ///< 输入缓冲区
    Buffer                   m_outputBuffer;     ///< 输出缓冲区
    OpenedCallback           m_openedCallback;   ///< 打开/关闭回调
    MessageCallback          m_messageCallback;  ///< 消息回调
};
}  // namespace zmuduo::net

#endif