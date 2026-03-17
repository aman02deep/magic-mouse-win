/**
 * power_manager.h — Phase 1
 * Subscribes to Windows power events via RegisterPowerSettingNotification.
 * Notifies DeviceManager on sleep/resume so HID handles are closed/reopened cleanly.
 */
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <atomic>

namespace magicmouse {

class DeviceManager;

class PowerManager {
public:
    explicit PowerManager(DeviceManager* dm);
    ~PowerManager();

    void start();
    void stop();

private:
    // Hidden window to receive WM_POWERBROADCAST messages
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void messageLoop();

    DeviceManager*  device_manager_;
    HWND            hwnd_ = nullptr;
    HPOWERNOTIFY    power_notify_ = nullptr;
    std::thread     msg_thread_;
    std::atomic<bool> running_{ false };
};

} // namespace magicmouse
