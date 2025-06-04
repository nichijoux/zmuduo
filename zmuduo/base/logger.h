// Original code - Copyright (c) sylar-yin
// Modified by nichijoux

#ifndef ZMUDUO_BASE_LOGGER_H
#define ZMUDUO_BASE_LOGGER_H

#include "zmuduo/base/singleton.h"
#include "zmuduo/base/thread.h"
#include "zmuduo/base/timestamp.h"

#include "zmuduo/base/mutex.h"
#include <condition_variable>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <queue>
#include <sstream>
#include <utility>
#include <vector>

#define ZMUDUO_LOGGER_ROOT zmuduo::LoggerMgr::GetInstance().getLogger()

#define ZMUDUO_LOG_BASE(level)                                                                     \
    if (ZMUDUO_LOGGER_ROOT && zmuduo::LogLevel::Level::level >= ZMUDUO_LOGGER_ROOT->getLevel())    \
    zmuduo::LogEventWrap(zmuduo::LogEvent(__LINE__, __FILE__, zmuduo::LogLevel::Level::level))     \
        .getEvent()                                                                                \
        .getStream()

#define ZMUDUO_LOG_DEBUG ZMUDUO_LOG_BASE(DEBUG)
#define ZMUDUO_LOG_INFO ZMUDUO_LOG_BASE(INFO)
#define ZMUDUO_LOG_WARNING ZMUDUO_LOG_BASE(WARNING)
#define ZMUDUO_LOG_IMPORTANT ZMUDUO_LOG_BASE(IMPORTANT)
#define ZMUDUO_LOG_ERROR ZMUDUO_LOG_BASE(ERROR)
#define ZMUDUO_LOG_FATAL ZMUDUO_LOG_BASE(FATAL)

#define ZMUDUO_LOG_FMT_BASE(level, fmt, ...)                                                       \
    if (ZMUDUO_LOGGER_ROOT && zmuduo::LogLevel::Level::level >= ZMUDUO_LOGGER_ROOT->getLevel())    \
    zmuduo::LogEventWrap(zmuduo::LogEvent(__LINE__, __FILE__, zmuduo::LogLevel::Level::level))     \
        .getEvent()                                                                                \
        .format(fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_DEBUG(fmt, ...) ZMUDUO_LOG_FMT_BASE(DEBUG, fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_INFO(fmt, ...) ZMUDUO_LOG_FMT_BASE(INFO, fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_WARNING(fmt, ...) ZMUDUO_LOG_FMT_BASE(WARNING, fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_IMPORTANT(fmt, ...) ZMUDUO_LOG_FMT_BASE(IMPORTANT, fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_ERROR(fmt, ...) ZMUDUO_LOG_FMT_BASE(ERROR, fmt, ##__VA_ARGS__)
#define ZMUDUO_LOG_FMT_FATAL(fmt, ...) ZMUDUO_LOG_FMT_BASE(FATAL, fmt, ##__VA_ARGS__)

namespace zmuduo {

class LogLevel {
  public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        IMPORTANT,
        ERROR,
        FATAL,
    };

    static std::string ToString(LogLevel::Level level);

    static LogLevel::Level FromString(const std::string& str);
};

class LogMode {
  public:
    enum Mode { STDOUT, FILE, BOTH };

    static std::string ToString(LogMode::Mode mode);

    static LogMode::Mode FromString(const std::string& string);
};

struct LogEvent {
  public:
    LogEvent(int32_t line, const char* file, LogLevel::Level level);

    /**
     * @brief 获取行号
     * @return 文件行号
     */
    int32_t getLine() const {
        return m_line;
    }

    pid_t getPid() const {
        return m_pid;
    }

    /**
     * @brief 获取线程id
     * @return 线程id
     */
    pid_t getTid() const {
        return m_tid;
    }

    /**
     * @brief 获取协程id
     * @return 协程id
     */
    int32_t getFiberId() const {
        return m_fiberId;
    }

    uint64_t getTime() const {
        return m_time;
    }

    const char* getFile() const {
        return m_file;
    }

    LogLevel::Level getLevel() const {
        return m_level;
    }

    std::string getLog();

    std::stringstream& getStream();

    /**
     * @brief 格式化写入日志内容
     * @param[in] fmt 格式字符串
     * @param[in] ... 待格式化数据
     */
    void format(const char* fmt, ...);

    /**
     * @brief 格式化写入日志内容
     * @param[in] fmt 格式字符串
     * @param[in] ... 待格式化数据
     */
    void format(const char* fmt, va_list al);

  private:
    // 行号
    int32_t m_line;
    // 进程 id
    pid_t m_pid;
    // 线程 id
    pid_t m_tid;
    // 协程 id
    int32_t m_fiberId;
    // 时间戳
    uint64_t m_time;
    // 文件名
    const char* m_file;
    // 日志级别
    LogLevel::Level m_level;
    // 日志内容
    std::stringstream m_ss;
};

class LogEventWrap {
  public:
    explicit LogEventWrap(LogEvent event) : m_event(std::move(event)) {}

    ~LogEventWrap();

    /**
     * @brief 获取日志事件
     */
    LogEvent& getEvent() {
        return m_event;
    }

  private:
    LogEvent m_event;
};

class AsyncLogger {
  public:
    using Ptr = std::shared_ptr<AsyncLogger>;

    AsyncLogger(LogMode::Mode mode, const char* filePath, int32_t maxSize, int64_t interval);

    ~AsyncLogger();

    void stop();

    void push(std::vector<std::string>& buffer);

    static void* mainLoop(void* arg);

    LogMode::Mode getMode() const {
        return m_mode;
    }

    int32_t getMaxSize() const {
        return m_maxSize;
    }

    int64_t getInterval() const {
        return m_interval;
    }

    int32_t getNo() const {
        return m_no;
    }

    const char* getFilePath() const {
        return m_filePath;
    }

    bool isStop() const {
        return m_stop;
    }

    bool isReOpen() const {
        return m_reopen;
    }

  public:
    std::queue<std::vector<std::string>> m_queue;

  private:
    const char* m_filePath;
    /// @brief 记录模式
    LogMode::Mode m_mode;
    int32_t       m_maxSize;
    int64_t       m_interval;
    int32_t       m_no;
    Date          m_date;
    /// @brief 线程类
    thread::Thread m_thread;

    bool m_stop;
    bool m_reopen;

    FILE* m_file;

    std::mutex              m_main_mutex;
    std::mutex              m_operator_mutex;
    std::condition_variable m_condition;
    Semaphore               m_semaphore;
};

class Logger {
  public:
    using Ptr = std::shared_ptr<Logger>;
    // 5MB, 500ms
    Logger(LogMode::Mode   mode,
           const char*     filePath,
           int32_t         maxSize,
           int64_t         interval,
           LogLevel::Level level);

    ~Logger();

    void log(const std::string& msg);

    LogLevel::Level getLevel() const {
        return m_level;
    }
    void setLevel(LogLevel::Level v) {
        m_level = v;
    }

  private:
    std::mutex      m_mutex;
    LogLevel::Level m_level;
    AsyncLogger     m_asyncLogger;

    std::vector<std::string> m_buffer;
};

class LoggerManager {
  public:
    LoggerManager() {
        if (m_isDefault) {
            reset();
        }
    }

    void reset(LogMode::Mode   mode     = LogMode::Mode::BOTH,
               const char*     filePath = "./",
               int32_t         maxSize  = 5 * 1024 * 1024,
               int64_t         interval = 500,
               LogLevel::Level level    = LogLevel::Level::DEBUG) {
#ifdef ZMUDUO_BENCK_MARK
        g_logger =
            std::make_shared<Logger>(mode, filePath, maxSize, interval, LogLevel::Level::ERROR);
#else
        g_logger = std::make_shared<Logger>(mode, filePath, maxSize, interval, level);
#endif
    }

    Logger::Ptr getLogger() const {
        return g_logger;
    }

    static void setDefaultLogger(bool v) {
        m_isDefault = v;
    }

  private:
    Logger::Ptr g_logger;
    static bool m_isDefault;
};

using LoggerMgr = Singleton<LoggerManager>;

}  // namespace zmuduo

#endif