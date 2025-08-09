#include "zmuduo/base/logger.h"

#include "zmuduo/base/utils/system_util.h"
#include <csignal>
#include <cstdarg>
#include <iostream>

using namespace zmuduo::utils::system_util;

namespace zmuduo {
void Exit(int sig) {
    exit(0);
}

struct SignalInternal {
    SignalInternal() {
        signal(SIGINT, Exit);
    }
};

static SignalInternal s_signal_internal;

// LogLevel
std::string LogLevel::ToString(LogLevel::Level level) {
    switch (level) {
#define CONVERT(level)                                                                             \
    case LogLevel::Level::level: return #level
        CONVERT(DEBUG);
        CONVERT(INFO);
        CONVERT(WARNING);
        CONVERT(IMPORTANT);
        CONVERT(ERROR);
        CONVERT(FATAL);
#undef CONVERT
        default: return "DEBUG";
    }
}

LogLevel::Level LogLevel::FromString(const std::string& s) {
#define CONVERT(level, str)                                                                        \
    if (s == #str) {                                                                               \
        return LogLevel::Level::level;                                                             \
    }

    CONVERT(DEBUG, debug)
    CONVERT(DEBUG, DEBUG)

    CONVERT(INFO, info)
    CONVERT(INFO, INFO)

    CONVERT(WARNING, warning)
    CONVERT(WARNING, WARNING)

    CONVERT(IMPORTANT, important)
    CONVERT(IMPORTANT, IMPORTANT)

    CONVERT(ERROR, error)
    CONVERT(ERROR, ERROR)

    CONVERT(FATAL, fatal)
    CONVERT(FATAL, FATAL)
#undef CONVERT

    return LogLevel::Level::DEBUG;
}

// LogMode
std::string LogMode::ToString(LogMode::Mode mode) {
    switch (mode) {
#define CONVERT(mode)                                                                              \
    case LogMode::Mode::mode: return #mode
        CONVERT(STDOUT);
        CONVERT(FILE);
        CONVERT(BOTH);
#undef CONVERT
        default: return "STDOUT";
    }
}

LogMode::Mode LogMode::FromString(const std::string& s) {
#define CONVERT(mode, str)                                                                         \
    if (s == #str) {                                                                               \
        return LogMode::Mode::mode;                                                                \
    }
    CONVERT(STDOUT, STDOUT)
    CONVERT(FILE, FILE)
    CONVERT(BOTH, BOTH)
#undef CONVERT

    return LogMode::Mode::STDOUT;
}

LogMessage::LogMessage(LogLevel::Level level,
                       std::string     content,
                       std::string     file,
                       int             line,
                       std::string     func)
    : m_level(level),
      m_content(std::move(content)),
      m_timestamp(Timestamp::Now()),
      m_pid(GetPid()),
      m_tid(GetTid()),
      m_filename(std::move(file)),
      m_line(line),
      m_function(std::move(func)) {}

AsyncLogger::AsyncLogger()
    : m_stop(false),
      m_minLevel(LogLevel::DEBUG),
      m_mode(LogMode::STDOUT),
      m_logFilePath("./"),
      m_maxFileSize(100 * 1024 * 1024),
      m_currentFileSize(0),
      m_fileIndex(0),
      m_enableColor(true),
      m_workerThread([this]() { processLogs(); }) {
    m_workerThread.start();
}

AsyncLogger::~AsyncLogger() {
    m_stop = true;
    m_condition.notify_all();
    if (!m_workerThread.isJoined()) {
        m_workerThread.join();
    }
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
}

AsyncLogger& AsyncLogger::GetInstance() {
    static AsyncLogger logger;
    return logger;
}

void AsyncLogger::reset(LogLevel::Level    level,
                        LogMode::Mode      mode,
                        const std::string& filepath,
                        size_t             maxFileSize,
                        bool               enableColor) {
    // 最小记录级别
    m_minLevel = level;
    // 记录模式
    m_mode = mode;
    // 是否显示颜色
    m_enableColor = enableColor;

    // 文件记录设置,是否需要重新打开文件
    if (filepath != m_logFilePath || maxFileSize != m_maxFileSize) {
        // 关闭当前文件
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        // 重置文件信息
        m_logFilePath     = filepath;
        m_maxFileSize     = maxFileSize;
        m_fileIndex       = 0;
        m_currentFileSize = 0;
        // 是否需要真的打开文件
        if (m_mode == LogMode::Mode::FILE || m_mode == LogMode::Mode::BOTH) {
            openLogFile();
        }
    } else {
        // 否则更新最大文件大小即可
        m_maxFileSize = maxFileSize;
    }
}

void AsyncLogger::log(LogLevel::Level    level,
                      const std::string& message,
                      const std::string& filename,
                      int                line,
                      const std::string& function) {
    // 日记级别分裂
    if (level < m_minLevel) {
        return;
    }
    // 创建日志信息
    LogMessage logMessage(level, message, filename, line, function);
    // 放入队列中准备打印
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_logQueue.emplace(std::move(logMessage));
    }
    m_condition.notify_one();
}

void AsyncLogger::logFormat(LogLevel::Level    level,
                            const std::string& filename,
                            int                line,
                            const std::string& function,
                            const char*        format,
                            ...) {
    if (level < m_minLevel) {
        return;
    }
    // 使用vasprintf确定buffer大小
    va_list args;
    va_start(args, format);
    char* buffer = nullptr;
    auto  size   = vasprintf(&buffer, format, args);
    if (size != -1) {
        log(level, std::string(buffer, size), filename, line, function);
        free(buffer);
    }
    va_end(args);
}

std::string AsyncLogger::GetLevelColor(LogLevel::Level level) {
    switch (level) {
        case LogLevel::Level::DEBUG: return color::CYAN;
        case LogLevel::Level::INFO: return color::BLUE;
        case LogLevel::Level::WARNING: return color::YELLOW;
        case LogLevel::Level::IMPORTANT: return color::GREEN;
        case LogLevel::Level::ERROR: return color::RED;
        case LogLevel::Level::FATAL: return color::BOLD_RED;
        default: return color::RESET;
    }
}

std::string AsyncLogger::FormatMessage(const LogMessage& message, bool useColor) {
    std::ostringstream oss;

    if (useColor) {
        oss << GetLevelColor(message.m_level);
    }

    oss << "[" << LogLevel::ToString(message.m_level) << "]" << "[" << message.m_timestamp << "]"
        << "[" << message.m_pid << "]" << "[" << message.m_tid << "]" << "[" << message.m_filename
        << ":" << message.m_line << "]" << "[" << message.m_function << "()]" << "---"
        << message.m_content;

    if (useColor) {
        oss << color::RESET;
    }

    return oss.str();
}

void AsyncLogger::openLogFile() {
    // 如果当前打开了文件则关闭
    if (m_fileStream.is_open()) {
        m_fileStream.close();
    }
    // 获取文件名
    std::stringstream ss;
    ss << m_logFilePath << Date::Now() << m_fileIndex << ".log";
    std::string filename = ss.str();
    // 追加方式打开文件
    m_fileStream.open(filename, std::ios::app);
    if (!m_fileStream.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }

    // 获取当前文件大小(判断是否需要切片)
    m_fileStream.seekp(0, std::ios::end);
    m_currentFileSize = m_fileStream.tellp();
}

void AsyncLogger::checkFileRotation() {
    if (m_currentFileSize >= m_maxFileSize) {
        m_fileIndex++;
        m_currentFileSize = 0;
        openLogFile();
    }
}

void AsyncLogger::processLogs() {
    while (!m_stop) {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_condition.wait(lock, [this] { return !m_logQueue.empty() || m_stop; });
        // 获取队列内容
        while (!m_logQueue.empty()) {
            LogMessage message = m_logQueue.front();
            m_logQueue.pop();
            lock.unlock();
            // 写入STDOUT
            if (m_mode != LogMode::FILE) {
                std::string colored_msg = FormatMessage(message, m_enableColor);
                std::cout << colored_msg << std::endl;
            }
            // 写入文件
            if (m_mode != LogMode::STDOUT) {
                // 尝试打开文件
                if (!m_fileStream.is_open()) {
                    openLogFile();
                }
                // 写入文件
                if (m_fileStream.is_open()) {
                    // 写入文件的不需要颜色
                    std::string plainMessage = FormatMessage(message, false);
                    m_fileStream << plainMessage << std::endl;
                    m_fileStream.flush();
                    // 检测是否需要文件切片
                    m_currentFileSize += plainMessage.length() + 1;
                    checkFileRotation();
                }
            }
            // 是否为FATAL
            if (message.m_level == LogLevel::Level::FATAL) {
                exit(0);
            }
            lock.lock();
        }
    }
}

LogStream::LogStream(LogLevel::Level level, std::string filename, int line, std::string function)
    : m_level(level),
      m_filename(std::move(filename)),
      m_line(line),
      m_function(std::move(function)) {}

LogStream::~LogStream() {
    AsyncLogger::GetInstance().log(m_level, m_oss.str(), m_filename, m_line, m_function);
}

}  // namespace zmuduo