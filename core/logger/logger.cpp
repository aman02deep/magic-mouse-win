#include "logger.h"

#include <windows.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace mm {

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────────────────

static std::string levelStr(LogLevel l) {
    switch (l) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERR:   return "ERROR";
        case LogLevel::FATAL: return "FATAL";
    }
    return "?????";
}

static std::string timestamp() {
    using namespace std::chrono;
    auto now  = system_clock::now();
    auto tt   = system_clock::to_time_t(now);
    auto ms   = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm buf{};
    localtime_s(&buf, &tt);

    std::ostringstream oss;
    oss << std::put_time(&buf, "%Y-%m-%d %H:%M:%S")
        << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return oss.str();
}

// ── Logger ───────────────────────────────────────────────────────────────────

Logger& Logger::get() {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& logDir, std::size_t maxBytes, int maxFiles) {
    std::lock_guard<std::mutex> lock(mutex_);

    logDir_   = logDir;
    maxBytes_ = maxBytes;
    maxFiles_ = maxFiles;

    fs::create_directories(logDir_);
    openCurrent();
    initialized_ = true;
}

void Logger::openCurrent() {
    currentPath_ = (fs::path(logDir_) / "magic_mouse.log").string();
    file_.open(currentPath_, std::ios::app | std::ios::binary);

    // Measure existing size so we don't rotate on first open of a small file.
    if (file_.is_open()) {
        bytesWritten_ = static_cast<std::size_t>(
            fs::exists(currentPath_) ? fs::file_size(currentPath_) : 0);
    }
}

void Logger::doRotate() {
    file_.close();

    fs::path base(logDir_);

    // Delete oldest if we're at the limit.
    fs::path oldest = base / ("magic_mouse." + std::to_string(maxFiles_) + ".log");
    if (fs::exists(oldest)) {
        fs::remove(oldest);
    }

    // Shift: magic_mouse.(N-1).log → magic_mouse.N.log
    for (int i = maxFiles_ - 1; i >= 1; --i) {
        fs::path from = base / ("magic_mouse." + std::to_string(i)     + ".log");
        fs::path to   = base / ("magic_mouse." + std::to_string(i + 1) + ".log");
        if (fs::exists(from)) {
            fs::rename(from, to);
        }
    }

    // magic_mouse.log → magic_mouse.1.log
    fs::path current(currentPath_);
    if (fs::exists(current)) {
        fs::rename(current, base / "magic_mouse.1.log");
    }

    bytesWritten_ = 0;
    file_.open(currentPath_, std::ios::out | std::ios::binary);
}

void Logger::rotateIfNeeded() {
    if (bytesWritten_ >= maxBytes_) {
        doRotate();
    }
}

void Logger::log(LogLevel level, const std::string& tag, const std::string& msg) {
    if (level < minLevel_) return;

    std::ostringstream line;
    line << timestamp()
         << " [" << levelStr(level) << "]"
         << " [" << tag << "] "
         << msg << '\n';

    std::string entry = line.str();

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // If init() was never called, fall back to stderr.
        if (!initialized_ || !file_.is_open()) {
            std::cerr << entry;
            return;
        }

        rotateIfNeeded();
        file_ << entry;
        file_.flush();
        bytesWritten_ += entry.size();
    }

    // Also mirror WARN+ to OutputDebugString for Visual Studio Output window.
    if (level >= LogLevel::WARN) {
        OutputDebugStringA(entry.c_str());
    }
}

} // namespace mm
