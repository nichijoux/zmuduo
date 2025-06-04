#include "logger.h"
#include "timestamp.h"
#include "zmuduo/base/utils/fs_util.h"
#include "zmuduo/base/utils/system_util.h"

#include <cassert>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>

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

// LogEvent
LogEvent::LogEvent(int32_t line, const char* file, LogLevel::Level level)
    : m_line(line),
      m_file(file),
      m_level(level),
      m_fiberId(0),
      m_pid(0),
      m_tid(0),
      m_time(0),
      m_ss() {}

void LogEvent::format(const char* fmt, ...) {
    va_list al;
    va_start(al, fmt);
    getStream();
    format(fmt, al);
    va_end(al);
}

void LogEvent::format(const char* fmt, va_list al) {
    char* buffer = nullptr;
    int   len    = vasprintf(&buffer, fmt, al);
    if (len != -1) {
        std::string s = std::string(buffer, len);
        m_ss << std::string(buffer, len);
        free(buffer);
    }
}

std::stringstream& LogEvent::getStream() {
    // 设置颜色
#define LevelColor(level, color)                                                                   \
    case LogLevel::Level::level: {                                                                 \
        m_ss << color << "[" << LogLevel::ToString(m_level) << "]";                                \
        break;                                                                                     \
    }

    switch (m_level) {
        LevelColor(INFO, "\033[34m")           // 蓝色
            LevelColor(ERROR, "\033[31m")      // 红色
            LevelColor(WARNING, "\033[33m")    // 黄色
            LevelColor(IMPORTANT, "\033[32m")  // 绿色
            LevelColor(DEBUG, "\033[36m")      // 青色
            default : m_ss
                      << "\033[0m[DEFAULT]";  // 默认颜色
        break;
    }
#undef LevelColor
    m_ss << "[" << Timestamp::Now() << "][" << utils::SystemUtil::GetPid() << "]["
         << utils::SystemUtil::GetThreadId() << "][" << utils::FSUtil::GetName(m_file) << ":"
         << m_line << "]---";
    return m_ss;
}

std::string LogEvent::getLog() {
    return m_ss.str() + "\n";
}

LogEventWrap::~LogEventWrap() {
    ZMUDUO_LOGGER_ROOT->log(m_event.getLog());
    if (m_event.getLevel() == LogLevel::Level::FATAL) {
        exit(0);
    }
}

// AsyncLogger
AsyncLogger::AsyncLogger(LogMode::Mode mode,
                         const char*   filePath,
                         int32_t       maxSize,
                         int64_t       interval)
    : m_filePath(filePath),
      m_mode(mode),
      m_maxSize(maxSize),
      m_interval(interval),
      m_no(0),
      m_date(Date::Now()),
      m_thread([this]() { mainLoop(this); }),
      m_stop(true),
      m_reopen(true),
      m_file(nullptr),
      m_semaphore() {
    m_thread.start();

    m_semaphore.wait();
}

AsyncLogger::~AsyncLogger() {
    stop();
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
}

void AsyncLogger::stop() {
    if (!m_stop) {
        m_stop = true;
        m_condition.notify_one();
        m_thread.join();
    }
}

void AsyncLogger::push(std::vector<std::string>& buffer) {
    {
        std::unique_lock<std::mutex> lock(m_operator_mutex);
        m_queue.push(buffer);
    }

    m_condition.notify_one();
}

void* AsyncLogger::mainLoop(void* arg) {
    auto* logger   = reinterpret_cast<AsyncLogger*>(arg);
    logger->m_stop = false;
    logger->m_semaphore.notify();

    while (true) {
        bool                     isStop;
        std::vector<std::string> msg;
        {
            std::unique_lock<std::mutex> lock(logger->m_main_mutex);
            while (logger->m_queue.empty() && !logger->m_stop) {
                logger->m_condition.wait(lock);
            }

            isStop = logger->m_stop;

            if (isStop && logger->m_queue.empty()) {
                break;
            }

            if (!logger->m_queue.empty()) {
                msg = logger->m_queue.front();
                logger->m_queue.pop();
            }
        }

        if (logger->m_mode != LogMode::Mode::FILE) {
            for (auto& i : msg) {
                std::cout << i;
            }
        }

        if (logger->m_mode != LogMode::Mode::STDOUT) {
            Date date = Date::Now();
            if (date != logger->m_date) {
                logger->m_no     = 0;
                logger->m_date   = date;
                logger->m_reopen = true;
                if (logger->m_file) {
                    fclose(logger->m_file);
                }
            }

            std::stringstream ss;
            ss << logger->m_filePath << logger->m_date << "_" << logger->m_no << ".log";

            if (logger->m_reopen || !logger->m_file) {
                // 创建文件并打开
                logger->m_file = fopen(ss.str().data(), "a");
                assert(logger->m_file);
                logger->m_reopen = false;
            }

            if (ftell(logger->m_file) > logger->m_maxSize) {
                fclose(logger->m_file);

                logger->m_no++;
                std::stringstream ss2;
                ss2 << logger->m_filePath << logger->m_date << "_" << logger->m_no << ".log";

                logger->m_file = fopen(ss2.str().c_str(), "a");
                assert(logger->m_file);
                logger->m_reopen = false;
            }

            for (auto& i : msg) {
                if (!i.empty()) {
                    fwrite(i.c_str(), 1, i.size(), logger->m_file);
                }
            }
            fflush(logger->m_file);
        }
    }

    if (logger->m_file) {
        fclose(logger->m_file);
        logger->m_file = nullptr;
    }

    return nullptr;
}

// Logger
Logger::Logger(LogMode::Mode   mode,
               const char*     filePath,
               int32_t         maxSize,
               int64_t         interval,
               LogLevel::Level level)
    : m_level(level), m_asyncLogger(mode, filePath, maxSize, interval) {}

Logger::~Logger() {
    m_asyncLogger.stop();
}

void Logger::log(const std::string& msg) {
    std::vector<std::string> tmp;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_buffer.push_back(msg);
        tmp.swap(m_buffer);
    }

    m_asyncLogger.push(tmp);
}

bool LoggerManager::m_isDefault = true;
}  // namespace zmuduo