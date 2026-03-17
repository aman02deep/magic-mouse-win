/**
 * device_manager.cpp — Phase 1
 *
 * Manages HID device: enumerate, connect, read loop, hot-plug retry.
 * Feeds raw HID packets to the input parser pipeline (Phase 2+).
 */
#include "device_manager.h"
#include "device_detector.h"
#include "hid_reader.h"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace magicmouse {

DeviceManager::DeviceManager() = default;
DeviceManager::~DeviceManager() { stop(); }

void DeviceManager::start() {
    running_ = true;
    reader_thread_ = std::thread([this] { readerLoop(); });
}

void DeviceManager::stop() {
    running_ = false;
    if (reader_thread_.joinable()) reader_thread_.join();
}

void DeviceManager::onSessionChange() {
    // TODO Phase 1: reload per-user config on logon
    std::cout << "[DeviceManager] Session change detected.\n";
}

void DeviceManager::onSuspend() {
    // Release HID handle before system sleeps — prevents stale handle on resume
    std::cout << "[DeviceManager] Suspend: releasing HID handles.\n";
    running_ = false;
    if (reader_thread_.joinable()) reader_thread_.join();
}

void DeviceManager::onResume() {
    // Re-enumerate and reconnect after wake
    std::cout << "[DeviceManager] Resume: re-enumerating devices.\n";
    retry_delay_ms_ = 500;
    running_ = true;
    reader_thread_ = std::thread([this] { readerLoop(); });
}

bool DeviceManager::isReaderAlive() const {
    return reader_alive_.load();
}

void DeviceManager::readerLoop() {
    while (running_) {
        // Enumerate
        auto devices = enumerateMagicMice();
        if (devices.empty()) {
            std::cout << "[DeviceManager] No Magic Mouse found. Retrying in "
                      << retry_delay_ms_ << "ms...\n";
            scheduleRetry();
            continue;
        }

        const auto& dev = devices[0]; // first_connected policy (Phase 2: full policy)
        active_device_path_ = dev.path;
        retry_delay_ms_ = 500; // reset back-off on successful connect

        std::cout << "[DeviceManager] Connecting to " << dev.model_name
                  << " (" << dev.serial << ")\n";

        // Open
        std::unique_ptr<HidReader> reader;
        try {
            reader = std::make_unique<HidReader>(dev.path);
        } catch (const std::exception& e) {
            std::cerr << "[DeviceManager] Open failed: " << e.what() << "\n";
            scheduleRetry();
            continue;
        }

        reader_alive_ = true;
        uint8_t buf[HidReader::kReportBufferSize];

        // Read loop
        while (running_) {
            int n = reader->read(buf, 100 /*ms*/);
            if (n < 0) {
                std::cerr << "[DeviceManager] Read error: " << reader->lastError() << "\n";
                break; // reconnect
            }
            if (n == 0) continue; // timeout, no data

            // TODO Phase 2: route buf to InputParser → GestureEngine → ScrollEngine
            // For now, just count packets (Phase 0/1 stub)
            (void)buf;
        }

        reader_alive_ = false;
        // Falls through to retry if running_ still true
        if (running_) scheduleRetry();
    }
}

void DeviceManager::scheduleRetry() {
    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms_));
    // Exponential back-off: 500 → 1000 → 2000 → 5000ms
    retry_delay_ms_ = (std::min)(retry_delay_ms_ * 2, kMaxRetryDelayMs);
}

} // namespace magicmouse
