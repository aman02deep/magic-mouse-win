/**
 * power_manager.cpp — Phase 1
 */
#include "power_manager.h"
#include "device_manager.h"

#include <iostream>

namespace magicmouse {

static DeviceManager* s_DeviceManager = nullptr; // set per-instance via WndProc trick

PowerManager::PowerManager(DeviceManager* dm)
    : device_manager_(dm) {}

PowerManager::~PowerManager() { stop(); }

void PowerManager::start() {
    running_ = true;
    msg_thread_ = std::thread([this] { messageLoop(); });
}

void PowerManager::stop() {
    running_ = false;
    if (hwnd_) {
        PostMessage(hwnd_, WM_QUIT, 0, 0);
    }
    if (msg_thread_.joinable()) msg_thread_.join();
}

LRESULT CALLBACK PowerManager::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_POWERBROADCAST) {
        auto* pm = reinterpret_cast<PowerManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (pm && pm->device_manager_) {
            switch (wp) {
            case PBT_APMSUSPEND:
                std::cout << "[PowerManager] System suspending.\n";
                pm->device_manager_->onSuspend();
                break;
            case PBT_APMRESUMEAUTOMATIC:
            case PBT_APMRESUMESUSPEND:
                std::cout << "[PowerManager] System resuming.\n";
                pm->device_manager_->onResume();
                break;
            default:
                break;
            }
        }
        return TRUE;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void PowerManager::messageLoop() {
    // Register a minimal hidden window class to receive WM_POWERBROADCAST
    WNDCLASSEX wc = {};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = "MagicMousePowerWnd";
    RegisterClassEx(&wc);

    hwnd_ = CreateWindowEx(0, "MagicMousePowerWnd", "", 0,
                           0, 0, 0, 0, HWND_MESSAGE, nullptr,
                           GetModuleHandle(nullptr), nullptr);
    if (!hwnd_) {
        std::cerr << "[PowerManager] Failed to create message window.\n";
        return;
    }

    // Store this pointer so WndProc can reach device_manager_
    SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    // Register for power setting notifications
    power_notify_ = RegisterPowerSettingNotification(
        hwnd_, &GUID_MONITOR_POWER_ON, DEVICE_NOTIFY_WINDOW_HANDLE);

    MSG message;
    while (running_ && GetMessage(&message, nullptr, 0, 0)) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    if (power_notify_) {
        UnregisterPowerSettingNotification(power_notify_);
        power_notify_ = nullptr;
    }
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

} // namespace magicmouse
