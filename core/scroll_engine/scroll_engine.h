/**
 * scroll_engine.h — Phase 2
 *
 * Implements macOS-style inertial scrolling physics for both vertical
 * and horizontal axes independently. Runs at 120Hz on a dedicated timer thread.
 * Calls InputInjector to deliver scroll events.
 */
#pragma once

#include <atomic>
#include <functional>
#include <thread>

namespace magicmouse {

struct ScrollParams {
    float decay_rate       = 0.92f;  // Per-tick velocity multiplier
    float sensitivity      = 1.2f;   // Input scale factor
    float max_speed        = 80.0f;  // Max velocity (px/tick)
    float dead_zone        = 0.5f;   // Stop if |velocity| < dead_zone
    bool  natural_scroll   = true;   // Invert direction
    bool  inertia_enabled  = true;
    bool  h_enabled        = true;
    float h_sensitivity    = 1.0f;
};

using ScrollOutputFn = std::function<void(float dv, float dh)>;

class ScrollEngine {
public:
    explicit ScrollEngine(ScrollOutputFn output_fn, ScrollParams params = {});
    ~ScrollEngine();

    /// Called by gesture engine during active finger touch
    void onScrollDelta(float dv, float dh);

    /// Called on finger lift — hands off current velocity to inertia
    void onFingerLift(float vel_v, float vel_h);

    /// Stop inertia immediately (e.g. finger touch-down)
    void cancelInertia();

    void setParams(const ScrollParams& p) { params_ = p; }

private:
    void tickLoop();

    ScrollOutputFn   output_fn_;
    ScrollParams     params_;

    std::atomic<float> vel_v_{ 0.0f };
    std::atomic<float> vel_h_{ 0.0f };
    std::atomic<bool>  inertia_active_{ false };
    std::atomic<bool>  running_{ false };

    std::thread tick_thread_;

    static constexpr int kTickHz = 120;
    static constexpr int kTickMs = 1000 / kTickHz;
};

} // namespace magicmouse
