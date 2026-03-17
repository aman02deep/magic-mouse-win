#include "hid_reader.h"
#include <hidapi.h>
#include <stdexcept>
#include <utility>

namespace magicmouse {

HidReader::HidReader(const std::string& device_path)
    : path_(device_path)
{
    // hid_init() is safe to call multiple times; hidapi tracks a reference count
    if (hid_init() != 0) {
        throw std::runtime_error("hid_init() failed");
    }

    device_ = hid_open_path(device_path.c_str());
    if (!device_) {
        const wchar_t* err = hid_error(nullptr);
        std::string msg = "Failed to open HID device: ";
        if (err) {
            // Convert wchar_t* to string for the error message
            std::wstring ws(err);
            std::string s(ws.length(), ' ');
            for (size_t i = 0; i < ws.length(); ++i) s[i] = static_cast<char>(ws[i]);
            msg += s;
        } else {
            msg += device_path;
        }
        throw std::runtime_error(msg);
    }

    // Set device to non-blocking so callers can pass a timeout to read()
    hid_set_nonblocking(device_, 0); // 0 = blocking; we handle timeout in read()
}

HidReader::~HidReader() {
    if (device_) {
        hid_close(device_);
        device_ = nullptr;
    }
    hid_exit();
}

HidReader::HidReader(HidReader&& other) noexcept
    : device_(other.device_), path_(std::move(other.path_))
{
    other.device_ = nullptr;
}

HidReader& HidReader::operator=(HidReader&& other) noexcept {
    if (this != &other) {
        if (device_) hid_close(device_);
        device_ = other.device_;
        path_ = std::move(other.path_);
        other.device_ = nullptr;
    }
    return *this;
}

int HidReader::read(uint8_t* buffer, int timeout_ms) {
    if (!device_) return -1;
    return hid_read_timeout(device_, buffer, kReportBufferSize, timeout_ms);
}

bool HidReader::isOpen() const {
    return device_ != nullptr;
}

std::string HidReader::lastError() const {
    const wchar_t* err = hid_error(device_);
    if (!err) return {};
    std::wstring ws(err);
    std::string s(ws.length(), ' ');
    for (size_t i = 0; i < ws.length(); ++i) s[i] = static_cast<char>(ws[i]);
    return s;
}

} // namespace magicmouse
