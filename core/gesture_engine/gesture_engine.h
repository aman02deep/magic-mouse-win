/**
 * gesture_engine.h — Phase 2
 *
 * State machine detecting gestures from touch events.
 * States: IDLE → TOUCH_START → TRACKING → SCROLL_LOCK | GESTURE_CANDIDATE → END
 */
#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace magicmouse {

/// Raw touch contact point from input_parser
struct TouchPoint {
    int     id;        // Finger tracking ID
    float   x, y;     // Coordinates on touch surface (pixels)
    float   pressure; // 0.0 – 1.0
};

/// Typed event emitted by the gesture engine
enum class GestureType {
    NONE,
    SWIPE_LEFT,
    SWIPE_RIGHT,
    SWIPE_UP,
    SWIPE_DOWN,
    SINGLE_TAP,
    TWO_FINGER_TAP,
    THREE_FINGER_TAP,
    DOUBLE_TAP,
    SCROLL_V,   // vertical scroll delta
    SCROLL_H,   // horizontal scroll delta
};

struct GestureEvent {
    GestureType type   = GestureType::NONE;
    float       delta  = 0.0f;  // For SCROLL_V / SCROLL_H deltas
    int         fingers = 0;
};

/// Callback invoked whenever the engine recognises a gesture
using GestureCallback = std::function<void(const GestureEvent&)>;

/// Parameters — all configurable via JSON config (see §7 in execution plan)
struct GestureParams {
    int   min_swipe_distance_px    = 8;
    int   max_swipe_time_ms        = 400;
    int   scroll_lock_threshold_px = 3;
    float gesture_velocity_ratio   = 2.0f;
    int   double_tap_interval_ms   = 250;
    int   tap_max_movement_px      = 3;
    int   gesture_lock_ms          = 300;
};

class GestureEngine {
public:
    explicit GestureEngine(GestureCallback cb, GestureParams params = {});

    /// Feed a snapshot of current touch contacts (call at HID poll rate ~60–250Hz)
    void onTouchFrame(const TouchPoint* points, int count, int64_t timestamp_ms);

    /// Notify engine that all fingers have lifted
    void onFingerLift(int64_t timestamp_ms);

    void setParams(const GestureParams& p) { params_ = p; }

private:
    enum class State {
        IDLE,
        TOUCH_START,
        TRACKING,
        SCROLL_LOCK,
        GESTURE_CANDIDATE,
        GESTURE_CONFIRMED,
        GESTURE_AMBIGUOUS,
    };

    void transition(State next);
    void classify(int64_t timestamp_ms);
    void emitScroll(float dx, float dy);

    GestureCallback callback_;
    GestureParams   params_;
    State           state_ = State::IDLE;

    // Tracking state
    float   start_x_        = 0, start_y_ = 0;
    float   last_x_         = 0, last_y_  = 0;
    float   velocity_x_     = 0, velocity_y_ = 0;
    int64_t touch_start_ms_ = 0;
    int64_t last_lift_ms_   = -10000;  // No recent lift initially
    int     finger_count_   = 0;
    int     tap_count_      = 0;
};

} // namespace magicmouse
