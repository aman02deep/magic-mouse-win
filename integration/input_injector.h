/**
 * input_injector.h — Phase 1 / Phase 2
 *
 * Thin wrapper around SendInput for scroll and gesture event injection.
 * Handles WHEEL/HWHEEL fractional accumulation.
 * Raw pointer X/Y movement is NOT injected here — that is handled
 * natively by the Boot Camp driver through the Windows HID stack.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace magicmouse {

/// Keystroke sequence (e.g. ALT+LEFT) represented as VK codes + flags
struct KeyCombo {
    std::vector<uint16_t> vk_codes;  // Virtual key codes to press in sequence
};

class InputInjector {
public:
    InputInjector();

    /// Inject a vertical scroll delta (can be fractional; accumulated internally)
    void injectScrollV(float delta);

    /// Inject a horizontal scroll delta (can be fractional; accumulated internally)
    void injectScrollH(float delta);

    /// Inject a key combo (e.g. Alt+Left for swipe back)
    void injectKeys(const KeyCombo& combo);

    /// Inject a mouse button click (left/right/middle)
    void injectClick(int button); // 0=left, 1=right, 2=middle

    /// Reset fractional accumulators (call on finger lift / inertia end)
    void resetAccumulators();

private:
    float accum_v_ = 0.0f;   // Vertical WHEEL_DELTA accumulator
    float accum_h_ = 0.0f;   // Horizontal HWHEEL_DELTA accumulator

    static constexpr float kWheelDelta = 120.0f; // Windows WHEEL_DELTA unit
};

} // namespace magicmouse
