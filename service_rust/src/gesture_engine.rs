#[derive(Clone, Copy, Debug)]
pub struct TouchPoint {
    pub id: i32,
    pub x: f32,
    pub y: f32,
    pub pressure: f32,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum GestureType {
    None,
    SwipeLeft,
    SwipeRight,
    SwipeUp,
    SwipeDown,
    SingleTap,
    TwoFingerTap,
    ThreeFingerTap,
    DoubleTap,
    ScrollV,
    ScrollH,
}

#[derive(Clone, Copy, Debug)]
pub struct GestureEvent {
    pub gtype: GestureType,
    pub delta: f32,
    pub fingers: i32,
}

#[derive(Clone, Copy, Debug)]
pub struct GestureParams {
    pub min_swipe_distance_px: i32,
    pub max_swipe_time_ms: i32,
    pub scroll_lock_threshold_px: i32,
    pub gesture_velocity_ratio: f32,
    pub double_tap_interval_ms: i32,
    pub tap_max_movement_px: i32,
    pub gesture_lock_ms: i32,
}

impl Default for GestureParams {
    fn default() -> Self {
        Self {
            min_swipe_distance_px: 8,
            max_swipe_time_ms: 400,
            scroll_lock_threshold_px: 3,
            gesture_velocity_ratio: 2.0,
            double_tap_interval_ms: 250,
            tap_max_movement_px: 3,
            gesture_lock_ms: 300,
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum State {
    Idle,
    TouchStart,
    Tracking,
    ScrollLock,
    GestureCandidate,
    GestureAmbiguous,
}

pub struct GestureEngine<F>
where
    F: Fn(GestureEvent),
{
    callback: F,
    params: GestureParams,
    state: State,

    start_x: f32,
    start_y: f32,
    last_x: f32,
    last_y: f32,
    velocity_x: f32,
    velocity_y: f32,
    touch_start_ms: i64,
    last_lift_ms: i64,
    finger_count: i32,
    tap_count: i32,
}

impl<F> GestureEngine<F>
where
    F: Fn(GestureEvent),
{
    pub fn new(cb: F, params: GestureParams) -> Self {
        Self {
            callback: cb,
            params,
            state: State::Idle,
            start_x: 0.0,
            start_y: 0.0,
            last_x: 0.0,
            last_y: 0.0,
            velocity_x: 0.0,
            velocity_y: 0.0,
            touch_start_ms: 0,
            last_lift_ms: -10000,
            finger_count: 0,
            tap_count: 0,
        }
    }

    pub fn set_params(&mut self, p: GestureParams) {
        self.params = p;
    }

    pub fn on_touch_frame(&mut self, points: &[TouchPoint], ts_ms: i64) {
        if points.is_empty() { return; }

        let x = points[0].x;
        let y = points[0].y;
        let count = points.len() as i32;

        match self.state {
            State::Idle | State::TouchStart => {
                self.start_x = x;
                self.start_y = y;
                self.last_x = x;
                self.last_y = y;
                self.velocity_x = 0.0;
                self.velocity_y = 0.0;
                self.finger_count = count;
                self.touch_start_ms = ts_ms;
                self.state = State::Tracking;
            }
            State::Tracking => {
                let dx = x - self.last_x;
                let dy = y - self.last_y;
                self.velocity_x = dx;
                self.velocity_y = dy;

                let total_dx = (x - self.start_x).abs();
                let total_dy = (y - self.start_y).abs();

                if total_dy >= self.params.scroll_lock_threshold_px as f32 && total_dy > total_dx {
                    self.state = State::ScrollLock;
                    self.emit_scroll(0.0, dy);
                } else if total_dx >= self.params.scroll_lock_threshold_px as f32 && total_dx > total_dy {
                    self.state = State::ScrollLock;
                    self.emit_scroll(dx, 0.0);
                } else if total_dx >= self.params.min_swipe_distance_px as f32 || total_dy >= self.params.min_swipe_distance_px as f32 {
                    let elapsed = ts_ms - self.touch_start_ms;
                    if elapsed < self.params.max_swipe_time_ms as i64 {
                        self.state = State::GestureCandidate;
                    }
                }

                self.last_x = x;
                self.last_y = y;
            }
            State::ScrollLock => {
                let dx = x - self.last_x;
                let dy = y - self.last_y;
                self.emit_scroll(dx, dy);
                self.last_x = x;
                self.last_y = y;
            }
            State::GestureCandidate => {
                self.classify(ts_ms);
            }
            _ => {}
        }
    }

    pub fn on_finger_lift(&mut self, ts_ms: i64) {
        if self.state == State::GestureCandidate || self.state == State::GestureAmbiguous {
            self.classify(ts_ms);
        } else if self.state == State::Tracking {
            let dx = self.last_x - self.start_x;
            let dy = self.last_y - self.start_y;
            let total_move = (dx * dx + dy * dy).sqrt();

            if total_move <= self.params.tap_max_movement_px as f32 {
                let since_last = ts_ms - self.last_lift_ms;
                if since_last <= self.params.double_tap_interval_ms as i64 {
                    (self.callback)(GestureEvent {
                        gtype: GestureType::DoubleTap,
                        delta: 0.0,
                        fingers: self.finger_count,
                    });
                    self.tap_count = 0;
                } else {
                    let gtype = match self.finger_count {
                        1 => GestureType::SingleTap,
                        2 => GestureType::TwoFingerTap,
                        _ => GestureType::ThreeFingerTap,
                    };
                    (self.callback)(GestureEvent {
                        gtype,
                        delta: 0.0,
                        fingers: self.finger_count,
                    });
                }
            }
        }

        self.last_lift_ms = ts_ms;
        self.state = State::Idle;
    }

    fn classify(&mut self, ts_ms: i64) {
        let dx = self.last_x - self.start_x;
        let dy = self.last_y - self.start_y;
        let adx = dx.abs();
        let ady = dy.abs();

        let since_lift = ts_ms - self.last_lift_ms;
        if since_lift < self.params.gesture_lock_ms as i64 {
            self.state = State::Idle;
            return;
        }

        let h_dominant = (adx > ady) && (self.velocity_x != 0.0) &&
            ((self.velocity_x.abs() / (self.velocity_y.abs() + 0.001)) >= self.params.gesture_velocity_ratio);
        
        let v_dominant = (ady > adx) && (self.velocity_y != 0.0) &&
            ((self.velocity_y.abs() / (self.velocity_x.abs() + 0.001)) >= self.params.gesture_velocity_ratio);

        let mut gtype = GestureType::None;

        if h_dominant && adx >= self.params.min_swipe_distance_px as f32 {
            gtype = if dx > 0.0 { GestureType::SwipeRight } else { GestureType::SwipeLeft };
        } else if v_dominant && ady >= self.params.min_swipe_distance_px as f32 {
            gtype = if dy > 0.0 { GestureType::SwipeDown } else { GestureType::SwipeUp };
        }

        if gtype != GestureType::None {
            (self.callback)(GestureEvent {
                gtype,
                delta: 0.0,
                fingers: self.finger_count,
            });
        }
        self.state = State::Idle;
    }

    fn emit_scroll(&self, dx: f32, dy: f32) {
        if dy != 0.0 {
            (self.callback)(GestureEvent {
                gtype: GestureType::ScrollV,
                delta: dy,
                fingers: self.finger_count,
            });
        }
        if dx != 0.0 {
            (self.callback)(GestureEvent {
                gtype: GestureType::ScrollH,
                delta: dx,
                fingers: self.finger_count,
            });
        }
    }
}
