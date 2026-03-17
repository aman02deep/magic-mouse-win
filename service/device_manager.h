/**
 * device_manager.h — Phase 1
 *
 * Manages HID device connection, disconnection, hot-plug,
 * and feeds raw packets to the processing pipeline.
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <atomic>
#include <thread>
#include <string>

namespace magicmouse {

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    void start();
    void stop();

    /// Called by SCM handler on SESSION_CHANGE
    void onSessionChange();

    /// Called by PowerManager on sleep/resume
    void onSuspend();
    void onResume();

    /// Returns true if the HID reader thread is healthy
    bool isReaderAlive() const;

private:
    void enumerateAndConnect();
    void readerLoop();
    void scheduleRetry();

    std::thread reader_thread_;
    std::atomic<bool> running_{ false };
    std::atomic<bool> reader_alive_{ false };

    std::string active_device_path_;
    int retry_delay_ms_ = 500;

    static constexpr int kMaxRetryDelayMs = 5000;
};

} // namespace magicmouse
