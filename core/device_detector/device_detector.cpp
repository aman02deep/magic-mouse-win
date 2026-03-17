#include "device_detector.h"
#include <hidapi.h>
#include <cstring>

namespace magicmouse {

namespace {
    std::string wideToUtf8(const wchar_t* wide) {
        if (!wide) return {};
        std::wstring ws(wide);
        return std::string(ws.begin(), ws.end());
    }

    std::string modelName(uint16_t pid) {
        switch (pid) {
            case kMagicMouse1Pid: return "Magic Mouse 1 (Bluetooth Classic)";
            case kMagicMouse2Pid: return "Magic Mouse 2 (BLE)";
            default:              return "Magic Mouse (unknown model)";
        }
    }
} // anonymous namespace

std::vector<DeviceInfo> enumerateMagicMice() {
    if (hid_init() != 0) return {};

    std::vector<DeviceInfo> results;

    // Enumerate all Apple devices (VID 0x05AC), then filter by known PIDs
    hid_device_info* devs = hid_enumerate(kAppleVendorId, 0);
    for (hid_device_info* cur = devs; cur != nullptr; cur = cur->next) {
        if (cur->product_id != kMagicMouse1Pid &&
            cur->product_id != kMagicMouse2Pid) {
            continue;
        }

        DeviceInfo info;
        info.vendor_id   = cur->vendor_id;
        info.product_id  = cur->product_id;
        info.serial      = wideToUtf8(cur->serial_number);
        info.path        = cur->path ? cur->path : "";
        info.model_name  = modelName(cur->product_id);
        results.push_back(std::move(info));
    }

    hid_free_enumeration(devs);
    return results;
}

} // namespace magicmouse
