/**
 * gesture_engine.cpp — Phase 2
 *
 * Full state machine implementation for gesture recognition.
 * See §4.6 of the execution plan for the complete state model.
 */
#include "gesture_engine.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace magicmouse {

GestureEngine::GestureEngine(GestureCallback cb, GestureParams params)
    : callback_(std::move(cb)), params_(params) {}

void GestureEngine::onTouchFrame(const TouchPoint* points, int count, int64_t ts_ms) {
    if (count == 0) return;

    float x = points[0].x;
    float y = points[0].y;

    switch (state_) {
    case State::IDLE:
    case State::TOUCH_START:
        start_x_ = x; start_y_ = y;
        last_x_ = x;  last_y_ = y;
        velocity_x_ = 0; velocity_y_ = 0;
        finger_count_ = count;
        touch_start_ms_ = ts_ms;
        transition(State::TRACKING);
        break;

    case State::TRACKING: {
        float dx = x - last_x_;
        float dy = y - last_y_;
        velocity_x_ = dx;
        velocity_y_ = dy;

        float total_dx = std::abs(x - start_x_);
        float total_dy = std::abs(y - start_y_);

        // Enter SCROLL_LOCK if vertical/horizontal movement dominates
        if (total_dy >= params_.scroll_lock_threshold_px && total_dy > total_dx) {
            transition(State::SCROLL_LOCK);
            emitScroll(0.0f, dy);
        } else if (total_dx >= params_.scroll_lock_threshold_px && total_dx > total_dy) {
            transition(State::SCROLL_LOCK);
            emitScroll(dx, 0.0f);
        }
        // Gesture candidate if swipe-like movement
        else if (total_dx >= params_.min_swipe_distance_px || total_dy >= params_.min_swipe_distance_px) {
            int64_t elapsed = ts_ms - touch_start_ms_;
            if (elapsed < params_.max_swipe_time_ms) {
                transition(State::GESTURE_CANDIDATE);
            }
        }
        last_x_ = x; last_y_ = y;
        break;
    }

    case State::SCROLL_LOCK: {
        float dx = x - last_x_;
        float dy = y - last_y_;
        emitScroll(dx, dy);
        last_x_ = x; last_y_ = y;
        break;
    }

    case State::GESTURE_CANDIDATE:
        classify(ts_ms);
        break;

    default:
        break;
    }
}

void GestureEngine::onFingerLift(int64_t ts_ms) {
    if (state_ == State::GESTURE_CANDIDATE || state_ == State::GESTURE_AMBIGUOUS) {
        classify(ts_ms);
    } else if (state_ == State::TRACKING) {
        // Tap detection
        float total_move = std::hypot(last_x_ - start_x_, last_y_ - start_y_);
        if (total_move <= params_.tap_max_movement_px) {
            int64_t since_last_lift = ts_ms - last_lift_ms_;
            if (since_last_lift <= params_.double_tap_interval_ms) {
                // Double tap
                GestureEvent ev;
                ev.type = GestureType::DOUBLE_TAP;
                ev.fingers = finger_count_;
                if (callback_) callback_(ev);
                tap_count_ = 0;
            } else {
                // Single/multi-finger tap based on finger count
                GestureEvent ev;
                ev.fingers = finger_count_;
                if (finger_count_ == 1)      ev.type = GestureType::SINGLE_TAP;
                else if (finger_count_ == 2) ev.type = GestureType::TWO_FINGER_TAP;
                else if (finger_count_ >= 3) ev.type = GestureType::THREE_FINGER_TAP;
                if (callback_) callback_(ev);
            }
        }
    }

    last_lift_ms_ = ts_ms;
    transition(State::IDLE);
}

void GestureEngine::transition(State next) {
    state_ = next;
}

void GestureEngine::classify(int64_t ts_ms) {
    float dx = last_x_ - start_x_;
    float dy = last_y_ - start_y_;
    float adx = std::abs(dx);
    float ady = std::abs(dy);

    // Check gesture_lock_ms: suppress gestures if we just came off a scroll
    int64_t since_lift = ts_ms - last_lift_ms_;
    if (since_lift < params_.gesture_lock_ms) {
        transition(State::IDLE);
        return;
    }

    // Velocity dominance check
    bool h_dominant = (adx > ady) &&
        (velocity_x_ != 0) &&
        (std::abs(velocity_x_) / (std::abs(velocity_y_) + 0.001f) >= params_.gesture_velocity_ratio);
    bool v_dominant = (ady > adx) &&
        (velocity_y_ != 0) &&
        (std::abs(velocity_y_) / (std::abs(velocity_x_) + 0.001f) >= params_.gesture_velocity_ratio);

    GestureEvent ev;
    if (h_dominant && adx >= params_.min_swipe_distance_px) {
        ev.type = (dx > 0) ? GestureType::SWIPE_RIGHT : GestureType::SWIPE_LEFT;
    } else if (v_dominant && ady >= params_.min_swipe_distance_px) {
        ev.type = (dy > 0) ? GestureType::SWIPE_DOWN : GestureType::SWIPE_UP;
    } else {
        ev.type = GestureType::NONE;
    }

    ev.fingers = finger_count_;
    if (ev.type != GestureType::NONE && callback_) callback_(ev);
    transition(State::IDLE);
}

void GestureEngine::emitScroll(float dx, float dy) {
    if (dy != 0.0f) {
        GestureEvent ev;
        ev.type = GestureType::SCROLL_V;
        ev.delta = dy;
        ev.fingers = finger_count_;
        if (callback_) callback_(ev);
    }
    if (dx != 0.0f) {
        GestureEvent ev;
        ev.type = GestureType::SCROLL_H;
        ev.delta = dx;
        ev.fingers = finger_count_;
        if (callback_) callback_(ev);
    }
}

} // namespace magicmouse
