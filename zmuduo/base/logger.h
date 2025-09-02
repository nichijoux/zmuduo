#ifndef ZMUDUO_BASE_LOGGER_H
#define ZMUDUO_BASE_LOGGER_H

#include "zmuduo/base/nomoveable.h"
#include "zmuduo/base/thread.h"
#include "zmuduo/base/timestamp.h"
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>

/**
 * @brief 获取文件名宏，提取路径中的文件名部分。
 * @note 使用 strrchr 获取最后一个 '/' 后的文件名。
 */
#define ZMUDUO_FILENAME_MACRO (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/**
 * @brief 获取函数名宏，兼容不同编译器。
 * @note 使用 __builtin_FUNCTION, __FUNCTION__, 或 __func__ 获取当前函数名。
 */
#if defined(__clang__) || defined(__GNUC__)
#    if __has_builtin(__builtin_FUNCTION)
#        define ZMUDUO_FUNCTION_MACRO __builtin_FUNCTION()
#    else
#        define ZMUDUO_FUNCTION_MACRO __func__
#    endif
#elif defined(_MSC_VER)
#    define ZMUDUO_FUNCTION_MACRO __FUNCTION__
#else
#    define ZMUDUO_FUNCTION_MACRO __func__  // C++11 标准
#endif

/**
 * @brief 基础日志宏，使用 LogStream 记录日志。
 * @param[in] level 日志级别（如 DEBUG, INFO）。
 * @note 包含文件名、行号和函数名，提交到 AsyncLogger。
 */
#define ZMUDUO_LOG_BASE(level)                                                                     \
    zmuduo::LogStream(zmuduo::LogLevel::level, ZMUDUO_FILENAME_MACRO, __LINE__,                    \
                      ZMUDUO_FUNCTION_MACRO)

/**
 * @brief DEBUG 级别日志宏。
 */
#define ZMUDUO_LOG_DEBUG ZMUDUO_LOG_BASE(DEBUG)

/**
 * @brief INFO 级别日志宏。
 */
#define ZMUDUO_LOG_INFO ZMUDUO_LOG_BASE(INFO)

/**
 * @brief WARNING 级别日志宏。
 */
#define ZMUDUO_LOG_WARNING ZMUDUO_LOG_BASE(WARNING)

/**
 * @brief IMPORTANT 级别日志宏。
 */
#define ZMUDUO_LOG_IMPORTANT ZMUDUO_LOG_BASE(IMPORTANT)

/**
 * @brief ERROR 级别日志宏。
 */
#define ZMUDUO_LOG_ERROR ZMUDUO_LOG_BASE(ERROR)

/**
 * @brief FATAL 级别日志宏。
 * @note FATAL 级别日志会导致程序退出。
 */
#define ZMUDUO_LOG_FATAL ZMUDUO_LOG_BASE(FATAL)

/**
 * @brief 基础格式化日志宏，支持 printf 风格。
 * @param[in] level 日志级别。
 * @param[in] format 格式化字符串。
 * @param[in] ... 可变参数。
 * @note 仅当级别高于 AsyncLogger 的最小级别时记录。
 */
#define ZMUDUO_LOG_FMT_BASE(level, format, ...)                                                    \
    if (zmuduo::LogLevel::level >= zmuduo::AsyncLogger::GetInstance().getLogLevel())               \
    zmuduo::AsyncLogger::GetInstance().logFormat(zmuduo::LogLevel::level, ZMUDUO_FILENAME_MACRO,   \
                                                 __LINE__, ZMUDUO_FUNCTION_MACRO, format,          \
                                                 ##__VA_ARGS__)

/**
 * @brief DEBUG 级别格式化日志宏。
 */
#define ZMUDUO_LOG_FMT_DEBUG(format, ...) ZMUDUO_LOG_FMT_BASE(DEBUG, format, ##__VA_ARGS__)

/**
 * @brief INFO 级别格式化日志宏。
 */
#define ZMUDUO_LOG_FMT_INFO(format, ...) ZMUDUO_LOG_FMT_BASE(INFO, format, ##__VA_ARGS__)

/**
 * @brief WARNING 级别格式化日志宏。
 */
#define ZMUDUO_LOG_FMT_WARNING(format, ...) ZMUDUO_LOG_FMT_BASE(WARNING, format, ##__VA_ARGS__)

/**
 * @brief IMPORTANT 级别格式化日志宏。
 */
#define ZMUDUO_LOG_FMT_IMPORTANT(format, ...) ZMUDUO_LOG_FMT_BASE(IMPORTANT, format, ##__VA_ARGS__)

/**
 * @brief ERROR 级别格式化日志宏。
 */
#define ZMUDUO_LOG_FMT_ERROR(format, ...) ZMUDUO_LOG_FMT_BASE(ERROR, format, ##__VA_ARGS__)

/**
 * @brief FATAL 级别格式化日志宏。
 * @note FATAL 级别日志会导致程序退出。
 */
#define ZMUDUO_LOG_FMT_FATAL(format, ...) ZMUDUO_LOG_FMT_BASE(FATAL, format, ##__VA_ARGS__)

namespace zmuduo {
/**
 * @namespace color
 * @brief ANSI 颜色代码，用于日志输出着色。
 */
namespace color {
    const std::string RESET   = "\033[0m";  ///< 重置颜色
    const std::string BLACK   = "\033[30m"; ///< 黑色
    const std::string RED     = "\033[31m"; ///< 红色
    const std::string GREEN   = "\033[32m"; ///< 绿色
    const std::string YELLOW  = "\033[33m"; ///< 黄色
    const std::string BLUE    = "\033[34m"; ///< 蓝色
    const std::string MAGENTA = "\033[35m"; ///< 品红色
    const std::string CYAN    = "\033[36m"; ///< 青色
    const std::string WHITE   = "\033[37m"; ///< 白色

    const std::string BRIGHT_BLACK   = "\033[90m"; ///< 亮黑色
    const std::string BRIGHT_RED     = "\033[91m"; ///< 亮红色
    const std::string BRIGHT_GREEN   = "\033[92m"; ///< 亮绿色
    const std::string BRIGHT_YELLOW  = "\033[93m"; ///< 亮黄色
    const std::string BRIGHT_BLUE    = "\033[94m"; ///< 亮蓝色
    const std::string BRIGHT_MAGENTA = "\033[95m"; ///< 亮品红色
    const std::string BRIGHT_CYAN    = "\033[96m"; ///< 亮青色
    const std::string BRIGHT_WHITE   = "\033[97m"; ///< 亮白色

    const std::string BOLD_RED     = "\033[1;31m"; ///< 加粗红色
    const std::string BOLD_GREEN   = "\033[1;32m"; ///< 加粗绿色
    const std::string BOLD_YELLOW  = "\033[1;33m"; ///< 加粗黄色
    const std::string BOLD_BLUE    = "\033[1;34m"; ///< 加粗蓝色
    const std::string BOLD_MAGENTA = "\033[1;35m"; ///< 加粗品红色
    const std::string BOLD_CYAN    = "\033[1;36m"; ///< 加粗青色
    const std::string BOLD_WHITE   = "\033[1;37m"; ///< 加粗白色
}                                                  // namespace color

/**
 * @class LogLevel
 * @brief 日志级别定义及字符串转换。
 *
 * 提供日志级别枚举（DEBUG, INFO, WARNING, IMPORTANT, ERROR, FATAL）及其与字符串的转换。
 */
class LogLevel {
public:
    /**
     * @enum Level
     * @brief 日志级别枚举。
     */
    enum Level {
        DEBUG,     ///< 调试信息
        INFO,      ///< 一般信息
        WARNING,   ///< 警告信息
        IMPORTANT, ///< 重要信息
        ERROR,     ///< 错误信息
        FATAL      ///< 致命错误，导致程序退出
    };

    /**
     * @brief 将日志级别转换为字符串。
     * @param[in] level 日志级别。
     * @return std::string 级别对应的字符串（如 "DEBUG"）。
     */
    static std::string ToString(Level level);

    /**
     * @brief 从字符串解析日志级别。
     * @param[in] s 级别字符串（如 "debug", "INFO"）。
     * @return LogLevel::Level 对应的日志级别，默认返回 DEBUG。
     */
    static Level FromString(const std::string& s);
};

/**
 * @class LogMode
 * @brief 日志输出模式定义及字符串转换。
 *
 * 提供日志输出模式枚举（STDOUT, FILE, BOTH）及其与字符串的转换。
 */
class LogMode {
public:
    /**
     * @enum Mode
     * @brief 日志输出模式枚举。
     */
    enum Mode {
        STDOUT, ///< 输出到标准输出
        FILE,   ///< 输出到文件
        BOTH    ///< 同时输出到标准输出和文件
    };

    /**
     * @brief 将输出模式转换为字符串。
     * @param[in] mode 输出模式。
     * @return std::string 模式对应的字符串（如 "STDOUT"）。
     */
    static std::string ToString(Mode mode);

    /**
     * @brief 从字符串解析输出模式。
     * @param[in] s 模式字符串（如 "stdout", "FILE"）。
     * @return LogMode::Mode 对应的输出模式，默认返回 STDOUT。
     */
    static Mode FromString(const std::string& s);
};

/**
 * @struct LogMessage
 * @brief 日志消息结构体，存储日志元数据。
 *
 * 包含日志级别、内容、时间戳、进程/线程 ID、文件名、行号和函数名。
 */
struct LogMessage {
    LogLevel::Level m_level;     ///< 日志级别
    std::string     m_content;   ///< 日志内容
    Timestamp       m_timestamp; ///< 日志时间戳
    pid_t           m_pid;       ///< 进程 ID
    pid_t           m_tid;       ///< 线程 ID
    std::string     m_filename;  ///< 文件名
    int             m_line;      ///< 行号
    std::string     m_function;  ///< 函数名

    /**
     * @brief 构造函数。
     * @param[in] level 日志级别。
     * @param[in] content 日志内容。
     * @param[in] file 文件名。
     * @param[in] line 行号。
     * @param[in] func 函数名。
     * @note 初始化时间戳、进程 ID 和线程 ID。
     */
    LogMessage(LogLevel::Level level,
               std::string     content,
               std::string     file,
               int             line,
               std::string     func);
};

/**
 * @class AsyncLogger
 * @brief 异步日志记录器，单例模式，支持多输出模式和级别过滤。
 *
 * AsyncLogger 使用后台线程异步处理日志，支持标准输出（STDOUT）、文件（FILE）或两者（BOTH）。
 * 提供颜色输出、文件切片和级别过滤功能，通过宏（如 ZMUDUO_LOG_INFO）或 LogStream 使用。
 *
 * @note AsyncLogger 线程安全，日志队列由 mutex 和 condition_variable 保护。
 * @note FATAL 级别日志会导致程序退出，需谨慎使用。
 * @note 日志队列高并发时可能延迟，需监控队列大小。
 *
 * @example
 * @code
 * #include "zmuduo/base/logger.h"
 * zmuduo::AsyncLogger::GetInstance().reset(zmuduo::LogLevel::INFO, zmuduo::LogMode::BOTH,
 * "./logs/", 10 * 1024 * 1024, true); ZMUDUO_LOG_INFO << "Starting application";
 * ZMUDUO_LOG_FMT_DEBUG("User %s logged in", "Alice");
 * @endcode
 */
class AsyncLogger : NoCopyable, NoMoveable {
public:
    /**
     * @brief 析构函数。
     * @note 停止后台线程，关闭文件流，清理资源。
     */
    ~AsyncLogger();

    /**
     * @brief 获取 AsyncLogger 单例实例。
     * @return AsyncLogger& 单例引用。
     */
    static AsyncLogger& GetInstance();

    /**
     * @brief 获取当前最小日志级别。
     * @return LogLevel::Level 最小日志级别。
     */
    LogLevel::Level getLogLevel() const {
        return m_minLevel;
    }

    /**
     * @brief 设置最小日志级别。
     * @param[in] level 最小日志级别。
     * @note 低于此级别的日志将被忽略。
     */
    void setLogLevel(const LogLevel::Level level) {
        m_minLevel = level;
    }

    /**
     * @brief 获取当前日志输出模式。
     * @return LogMode::Mode 日志输出模式。
     */
    LogMode::Mode getLogMode() const {
        return m_mode;
    }

    /**
     * @brief 设置日志输出模式。
     * @param[in] mode 日志输出模式（STDOUT, FILE, BOTH）。
     */
    void setLogMode(const LogMode::Mode mode) {
        m_mode = mode;
    }

    /**
     * @brief 设置日志文件路径。
     * @param[in] filepath 文件路径。
     * @note 关闭当前文件流，重置文件索引和大小。
     */
    void setLogFile(const std::string& filepath) {
        m_logFilePath = filepath;
        if (m_fileStream.is_open()) {
            m_fileStream.close();
        }
        m_fileIndex       = 0;
        m_currentFileSize = 0;
    }

    /**
     * @brief 设置最大日志文件大小。
     * @param[in] size 最大文件大小（字节）。
     * @note 超过此大小将触发文件切片。
     */
    void setMaxFileSize(const size_t size) {
        m_maxFileSize = size;
    }

    /**
     * @brief 启用或禁用颜色输出。
     * @param[in] enabled 是否启用颜色。
     * @note 仅对 STDOUT 有效，文件输出不含颜色。
     */
    void setColorEnabled(const bool enabled) {
        m_enableColor = enabled;
    }

    /**
     * @brief 检查是否启用颜色输出。
     * @retval true 颜色输出已启用。
     * @retval false 颜色输出已禁用。
     */
    bool isColorEnabled() const {
        return m_enableColor;
    }

    /**
     * @brief 重置日志配置。
     * @param[in] level 最小日志级别（默认 DEBUG）。
     * @param[in] mode 输出模式（默认 STDOUT）。
     * @param[in] filepath 文件路径（默认 "./"）。
     * @param[in] maxFileSize 最大文件大小（默认 100MB）。
     * @param[in] enableColor 是否启用颜色（默认 true）。
     * @note 关闭当前文件流，重新打开文件（如需要）。
     */
    void reset(LogLevel::Level    level       = LogLevel::DEBUG,
               LogMode::Mode      mode        = LogMode::STDOUT,
               const std::string& filepath    = "./",
               size_t             maxFileSize = 100 * 1024 * 1024,
               bool               enableColor = true);

    /**
     * @brief 记录日志消息。
     * @param[in] level 日志级别。
     * @param[in] message 日志内容。
     * @param[in] filename 文件名。
     * @param[in] line 行号。
     * @param[in] function 函数名。
     * @note 若级别低于 m_minLevel，则忽略；否则加入日志队列。
     */
    void log(LogLevel::Level    level,
             const std::string& message,
             const std::string& filename,
             int                line,
             const std::string& function);

    /**
     * @brief 记录格式化日志消息。
     * @param[in] level 日志级别。
     * @param[in] filename 文件名。
     * @param[in] line 行号。
     * @param[in] function 函数名。
     * @param[in] format 格式化字符串。
     * @param[in] ... 可变参数。
     * @note 使用 vasprintf 格式化，若级别低于 m_minLevel，则忽略。
     */
    void logFormat(LogLevel::Level    level,
                   const std::string& filename,
                   int                line,
                   const std::string& function,
                   const char*        format,
                   ...);

    /**
     * @brief 刷新所有待处理日志。
     * @note 阻塞直到日志队列清空。
     */
    void flush() {
        std::unique_lock lock(m_queueMutex);
        m_condition.wait(lock, [this] { return m_logQueue.empty(); });
    }

private:
    /**
     * @brief 构造函数，初始化单例。
     * @note 启动后台线程，默认配置为 DEBUG 级别、STDOUT 模式、100MB 文件大小。
     */
    AsyncLogger();

    /**
     * @brief 获取日志级别的颜色代码。
     * @param[in] level 日志级别。
     * @return std::string ANSI 颜色代码。
     */
    static std::string GetLevelColor(LogLevel::Level level);

    /**
     * @brief 格式化日志消息。
     * @param[in] message 日志消息。
     * @param[in] useColor 是否启用颜色。
     * @return std::string 格式化后的日志字符串。
     */
    static std::string FormatMessage(const LogMessage& message, bool useColor = false);

    /**
     * @brief 打开日志文件。
     * @note 关闭当前文件流，生成新文件名（含日期和索引），追加模式打开。
     */
    void openLogFile();

    /**
     * @brief 检查是否需要文件切片。
     * @note 若当前文件大小超过 m_maxFileSize，则增加索引并重新打开文件。
     */
    void checkFileRotation();

    /**
     * @brief 后台线程处理日志队列。
     * @note 循环提取日志，输出到 STDOUT 或文件，处理 FATAL 级别退出。
     */
    void processLogs();

private:
    std::queue<LogMessage>  m_logQueue;                            ///< 日志消息队列
    std::mutex              m_queueMutex;                          ///< 队列互斥锁
    std::condition_variable m_condition;                           ///< 队列条件变量
    thread::Thread          m_workerThread;                        ///< 后台处理线程
    std::atomic<bool>       m_stop        = false;                 ///< 停止标志
    LogLevel::Level         m_minLevel    = LogLevel::DEBUG;       ///< 最小日志级别
    LogMode::Mode           m_mode        = LogMode::STDOUT;       ///< 日志输出模式
    std::string             m_logFilePath = "./";                  ///< 日志文件路径
    std::ofstream           m_fileStream;                          ///< 日志文件流
    size_t                  m_maxFileSize     = 100 * 1024 * 1024; ///< 最大文件大小
    size_t                  m_currentFileSize = 0;                 ///< 当前文件大小
    int                     m_fileIndex       = 0;                 ///< 文件索引
    bool                    m_enableColor     = true;              ///< 是否启用颜色输出
};

/**
 * @class LogStream
 * @brief RAII 风格的日志流，简化流式日志记录。
 *
 * LogStream 使用流式操作符（<<）收集日志内容，析构时自动提交到 AsyncLogger。
 *
 * @note 日志提交在析构时进行，需确保 LogStream 生命周期正确。
 */
class LogStream {
public:
    /**
     * @brief 构造函数。
     * @param[in] level 日志级别。
     * @param[in] filename 文件名。
     * @param[in] line 行号。
     * @param[in] function 函数名。
     */
    LogStream(LogLevel::Level level, std::string filename, int line, std::string function);

    /**
     * @brief 析构函数。
     * @note 将收集的日志内容提交到 AsyncLogger。
     */
    ~LogStream();

    /**
     * @brief 流式操作符，收集日志内容。
     * @param[in] value 要记录的值。
     * @return LogStream& 当前对象，支持链式操作。
     */
    template <typename T>
    LogStream& operator<<(const T& value) {
        m_oss << value;
        return *this;
    }

private:
    LogLevel::Level    m_level;    ///< 日志级别
    std::string        m_filename; ///< 文件名
    int                m_line;     ///< 行号
    std::string        m_function; ///< 函数名
    std::ostringstream m_oss;      ///< 流缓冲区
};
} // namespace zmuduo
#endif  // ZMUDUO_BASE_LOGGER_H