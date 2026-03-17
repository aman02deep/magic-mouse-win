#pragma once

#include <hidapi.h>
#include <string>
#include <cstdint>

namespace magicmouse {

/// Wraps a single HIDAPI device handle.
/// Opens the device on construction, closes on destruction.
class HidReader {
public:
    static constexpr int kReportBufferSize = 64;

    /// Open the HID device at the given OS path (from hid_enumerate).
    /// Throws std::runtime_error on failure.
    explicit HidReader(const std::string& device_path);
    ~HidReader();

    // Non-copyable, movable
    HidReader(const HidReader&) = delete;
    HidReader& operator=(const HidReader&) = delete;
    HidReader(HidReader&&) noexcept;
    HidReader& operator=(HidReader&&) noexcept;

    /// Read one HID report into buffer.
    /// @param buffer  Output buffer (must be at least kReportBufferSize bytes)
    /// @param timeout_ms  Read timeout in milliseconds; 0 = nonblocking, -1 = blocking
    /// @return Number of bytes read, 0 on timeout, -1 on error
    int read(uint8_t* buffer, int timeout_ms = 100);

    /// Returns true if the device handle is open.
    bool isOpen() const;

    /// Returns the last HIDAPI error string (empty if no error).
    std::string lastError() const;

private:
    hid_device* device_ = nullptr;
    std::string path_;
};

} // namespace magicmouse
