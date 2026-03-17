/**
 * watchdog.h — Phase 1
 * Monitors the HID reader thread health. If the reader stalls for >5s
 * while a device is connected, restarts only the pipeline — not the whole service.
 */
#pragma once

#include <atomic>
#include <thread>
#include <chrono>

namespace magicmouse {

class DeviceManager;

class Watchdog {
public:
    explicit Watchdog(DeviceManager* dm);
    ~Watchdog();

    void start();
    void stop();

private:
    void watchLoop();

    DeviceManager*    device_manager_;
    std::thread       watch_thread_;
    std::atomic<bool> running_{ false };

    static constexpr int kStallThresholdSec = 5;
    static constexpr int kPollIntervalSec   = 1;
};

} // namespace magicmouse
