/**
 * input_injector.cpp — Phase 1 / Phase 2
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "input_injector.h"
#include <cmath>
#include <iostream>

namespace magicmouse {

InputInjector::InputInjector() = default;

void InputInjector::injectScrollV(float delta) {
    accum_v_ += delta;
    int whole = static_cast<int>(accum_v_ / kWheelDelta);
    if (whole != 0) {
        accum_v_ -= whole * kWheelDelta;
        INPUT inp = {};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_WHEEL;
        inp.mi.mouseData = static_cast<DWORD>(whole * static_cast<int>(kWheelDelta));
        SendInput(1, &inp, sizeof(INPUT));
    }
}

void InputInjector::injectScrollH(float delta) {
    accum_h_ += delta;
    int whole = static_cast<int>(accum_h_ / kWheelDelta);
    if (whole != 0) {
        accum_h_ -= whole * kWheelDelta;
        INPUT inp = {};
        inp.type = INPUT_MOUSE;
        inp.mi.dwFlags = MOUSEEVENTF_HWHEEL;
        inp.mi.mouseData = static_cast<DWORD>(whole * static_cast<int>(kWheelDelta));
        SendInput(1, &inp, sizeof(INPUT));
    }
}

void InputInjector::injectKeys(const KeyCombo& combo) {
    std::vector<INPUT> inputs;
    inputs.reserve(combo.vk_codes.size() * 2);

    // Key down
    for (uint16_t vk : combo.vk_codes) {
        INPUT inp = {};
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = vk;
        inputs.push_back(inp);
    }
    // Key up (reverse order)
    for (auto it = combo.vk_codes.rbegin(); it != combo.vk_codes.rend(); ++it) {
        INPUT inp = {};
        inp.type = INPUT_KEYBOARD;
        inp.ki.wVk = *it;
        inp.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(inp);
    }

    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void InputInjector::injectClick(int button) {
    INPUT inputs[2] = {};
    DWORD down_flag, up_flag;
    switch (button) {
    case 1:  down_flag = MOUSEEVENTF_RIGHTDOWN;  up_flag = MOUSEEVENTF_RIGHTUP;  break;
    case 2:  down_flag = MOUSEEVENTF_MIDDLEDOWN; up_flag = MOUSEEVENTF_MIDDLEUP; break;
    default: down_flag = MOUSEEVENTF_LEFTDOWN;   up_flag = MOUSEEVENTF_LEFTUP;   break;
    }
    inputs[0].type = INPUT_MOUSE; inputs[0].mi.dwFlags = down_flag;
    inputs[1].type = INPUT_MOUSE; inputs[1].mi.dwFlags = up_flag;
    SendInput(2, inputs, sizeof(INPUT));
}

void InputInjector::resetAccumulators() {
    accum_v_ = 0.0f;
    accum_h_ = 0.0f;
}

} // namespace magicmouse
