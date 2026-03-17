/**
 * scroll_engine.cpp — Phase 2
 *
 * Inertia physics running at 120Hz on a dedicated tick thread.
 * Handles vertical and horizontal axes independently.
 */
#include "scroll_engine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace magicmouse {

ScrollEngine::ScrollEngine(ScrollOutputFn output_fn, ScrollParams params)
    : output_fn_(std::move(output_fn)), params_(params)
{
    running_ = true;
    tick_thread_ = std::thread([this] { tickLoop(); });
}

ScrollEngine::~ScrollEngine() {
    running_ = false;
    if (tick_thread_.joinable()) tick_thread_.join();
}

void ScrollEngine::onScrollDelta(float dv, float dh) {
    // Called during active finger contact — direct pass-through scaled by sensitivity
    float sv = params_.natural_scroll ? -dv : dv;
    float sh = params_.h_enabled ? (params_.natural_scroll ? -dh : dh) : 0.0f;
    sv *= params_.sensitivity;
    sh *= params_.h_sensitivity;
    if (output_fn_) output_fn_(sv, sh);
}

void ScrollEngine::onFingerLift(float vel_v, float vel_h) {
    if (!params_.inertia_enabled) {
        vel_v_ = 0.0f;
        vel_h_ = 0.0f;
        inertia_active_ = false;
        return;
    }

    // Apply direction and clamp to max_speed
    float sv = params_.natural_scroll ? -vel_v : vel_v;
    float sh = params_.h_enabled ? (params_.natural_scroll ? -vel_h : vel_h) : 0.0f;
    sv = std::clamp(sv * params_.sensitivity, -params_.max_speed, params_.max_speed);
    sh = std::clamp(sh * params_.h_sensitivity, -params_.max_speed, params_.max_speed);

    vel_v_.store(sv);
    vel_h_.store(sh);
    inertia_active_ = true;
}

void ScrollEngine::cancelInertia() {
    vel_v_ = 0.0f;
    vel_h_ = 0.0f;
    inertia_active_ = false;
}

void ScrollEngine::tickLoop() {
    using namespace std::chrono;
    auto next = steady_clock::now();

    while (running_) {
        next += milliseconds(kTickMs);
        std::this_thread::sleep_until(next);

        if (!inertia_active_) continue;

        float v = vel_v_.load();
        float h = vel_h_.load();

        v *= params_.decay_rate;
        h *= params_.decay_rate;

        bool v_done = std::abs(v) < params_.dead_zone;
        bool h_done = std::abs(h) < params_.dead_zone;

        if (v_done) v = 0.0f;
        if (h_done) h = 0.0f;

        vel_v_.store(v);
        vel_h_.store(h);

        if (v_done && h_done) {
            inertia_active_ = false;
            continue;
        }

        if (output_fn_) output_fn_(v, h);
    }
}

} // namespace magicmouse
