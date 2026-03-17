#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace magicmouse {

/// Known Apple Magic Mouse PIDs
constexpr uint16_t kAppleVendorId    = 0x05AC;
constexpr uint16_t kMagicMouse1Pid   = 0x030D;  // Bluetooth Classic
constexpr uint16_t kMagicMouse2Pid   = 0x0269;  // BLE

/// Describes a detected HID device
struct DeviceInfo {
    uint16_t    vendor_id;
    uint16_t    product_id;
    std::string serial;       // Bluetooth MAC address e.g. "XX:XX:XX:XX:XX:XX"
    std::string path;         // OS-level HID path for hid_open_path()
    std::string model_name;   // Human-readable: "Magic Mouse 1" / "Magic Mouse 2"
};

/// Enumerate all connected Magic Mouse devices.
/// Calls hid_init() internally; safe to call multiple times.
/// Returns an empty vector if no Magic Mouse is paired and connected.
std::vector<DeviceInfo> enumerateMagicMice();

} // namespace magicmouse
