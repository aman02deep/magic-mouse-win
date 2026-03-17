#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <cstdint>

namespace mm {

enum class LogLevel {
    TRACE = 0,
    DEBUG,
    INFO,
    WARN,
    ERR,
    FATAL
};

/// Thread-safe, rotating file logger.
///
/// Usage:
///   Logger::get().init("C:\\ProgramData\\MagicMouse\\logs");
///   LOG_INFO("service", "started");
///
/// Rotation happens when the current log file exceeds maxBytes.
/// Old files are renamed: magic_mouse.log → magic_mouse.1.log → …
/// Files beyond maxFiles are deleted.
class Logger {
public:
    static Logger& get();

    /// Must be called once before any logging.
    /// @param logDir   Directory to write log files into (created if absent).
    /// @param maxBytes Rotate when current file exceeds this size (default: 5 MiB).
    /// @param maxFiles Maximum number of rotated files to keep (default: 5).
    void init(const std::string& logDir,
              std::size_t maxBytes = 5ULL * 1024 * 1024,
              int         maxFiles = 5);

    void log(LogLevel level, const std::string& tag, const std::string& msg);

    /// Convenience: set minimum level — messages below this are discarded.
    void setLevel(LogLevel level) { minLevel_ = level; }

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger() = default;

    void openCurrent();
    void rotateIfNeeded();
    void doRotate();

    std::mutex      mutex_;
    std::ofstream   file_;
    std::string     logDir_;
    std::string     currentPath_;
    std::size_t     maxBytes_  = 5ULL * 1024 * 1024;
    int             maxFiles_  = 5;
    std::size_t     bytesWritten_ = 0;
    LogLevel        minLevel_  = LogLevel::TRACE;
    bool            initialized_ = false;
};

} // namespace mm

// ── Convenience macros ────────────────────────────────────────────────────────

#define LOG_TRACE(tag, msg) ::mm::Logger::get().log(::mm::LogLevel::TRACE, tag, msg)
#define LOG_DEBUG(tag, msg) ::mm::Logger::get().log(::mm::LogLevel::DEBUG, tag, msg)
#define LOG_INFO(tag,  msg) ::mm::Logger::get().log(::mm::LogLevel::INFO,  tag, msg)
#define LOG_WARN(tag,  msg) ::mm::Logger::get().log(::mm::LogLevel::WARN,  tag, msg)
#define LOG_ERR(tag,   msg) ::mm::Logger::get().log(::mm::LogLevel::ERR,   tag, msg)
#define LOG_FATAL(tag, msg) ::mm::Logger::get().log(::mm::LogLevel::FATAL, tag, msg)
