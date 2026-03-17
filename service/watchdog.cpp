/**
 * watchdog.cpp — Phase 1
 */
#include "watchdog.h"
#include "device_manager.h"

#include <iostream>
#include <chrono>
#include <thread>

namespace magicmouse {

Watchdog::Watchdog(DeviceManager* dm) : device_manager_(dm) {}
Watchdog::~Watchdog() { stop(); }

void Watchdog::start() {
    running_ = true;
    watch_thread_ = std::thread([this] { watchLoop(); });
}

void Watchdog::stop() {
    running_ = false;
    if (watch_thread_.joinable()) watch_thread_.join();
}

void Watchdog::watchLoop() {
    int stall_count = 0;
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(kPollIntervalSec));

        if (!device_manager_->isReaderAlive()) {
            ++stall_count;
            if (stall_count >= kStallThresholdSec) {
                std::cerr << "[Watchdog] Reader stalled for " << stall_count
                          << "s — triggering reconnect.\n";
                // Restart the pipeline (not the full service)
                device_manager_->stop();
                device_manager_->start();
                stall_count = 0;
            }
        } else {
            stall_count = 0;
        }
    }
}

} // namespace magicmouse
