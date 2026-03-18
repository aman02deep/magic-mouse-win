use std::sync::{Arc, Mutex};
use std::thread;
use std::time::{Duration, Instant};

#[derive(Clone, Copy, Debug)]
pub struct ScrollParams {
    pub decay_rate: f32,
    pub sensitivity: f32,
    pub max_speed: f32,
    pub dead_zone: f32,
    pub natural_scroll: bool,
    pub inertia_enabled: bool,
    pub h_enabled: bool,
    pub h_sensitivity: f32,
}

impl Default for ScrollParams {
    fn default() -> Self {
        Self {
            decay_rate: 0.92,
            sensitivity: 1.2,
            max_speed: 80.0,
            dead_zone: 0.5,
            natural_scroll: true,
            inertia_enabled: true,
            h_enabled: true,
            h_sensitivity: 1.0,
        }
    }
}

struct State {
    vel_v: f32,
    vel_h: f32,
    inertia_active: bool,
    running: bool,
    params: ScrollParams,
}

pub struct ScrollEngine {
    state: Arc<Mutex<State>>,
    tick_thread: Option<thread::JoinHandle<()>>,
}

impl ScrollEngine {
    pub fn new<F>(output_fn: F, params: ScrollParams) -> Self
    where
        F: Fn(f32, f32) + Send + 'static,
    {
        let state = Arc::new(Mutex::new(State {
            vel_v: 0.0,
            vel_h: 0.0,
            inertia_active: false,
            running: true,
            params,
        }));

        let thread_state = Arc::clone(&state);
        let tick_thread = thread::spawn(move || {
            let tick_ms = 1000 / 120; // 120Hz
            let tick_duration = Duration::from_millis(tick_ms);
            let mut next = Instant::now();

            loop {
                next += tick_duration;
                let now = Instant::now();
                if next > now {
                    thread::sleep(next - now);
                } else {
                    next = now; // Catch up if behind
                }

                let mut s = thread_state.lock().unwrap();
                if !s.running {
                    break;
                }

                if !s.inertia_active {
                    continue;
                }

                s.vel_v *= s.params.decay_rate;
                s.vel_h *= s.params.decay_rate;

                let v_done = s.vel_v.abs() < s.params.dead_zone;
                let h_done = s.vel_h.abs() < s.params.dead_zone;

                if v_done { s.vel_v = 0.0; }
                if h_done { s.vel_h = 0.0; }

                if v_done && h_done {
                    s.inertia_active = false;
                    continue;
                }

                let (v, h) = (s.vel_v, s.vel_h);
                // Drop lock before calling external callback to prevent deadlocks
                drop(s);
                output_fn(v, h);
            }
        });

        Self {
            state,
            tick_thread: Some(tick_thread),
        }
    }

    pub fn on_scroll_delta<F>(&self, dv: f32, dh: f32, output_fn: &F)
    where
        F: Fn(f32, f32),
    {
        let s = self.state.lock().unwrap();
        let sv = if s.params.natural_scroll { -dv } else { dv };
        let sh = if s.params.h_enabled {
            if s.params.natural_scroll { -dh } else { dh }
        } else {
            0.0
        };
        
        let sens = s.params.sensitivity;
        let h_sens = s.params.h_sensitivity;
        drop(s);

        output_fn(sv * sens, sh * h_sens);
    }

    pub fn on_finger_lift(&self, vel_v: f32, vel_h: f32) {
        let mut s = self.state.lock().unwrap();
        if !s.params.inertia_enabled {
            s.vel_v = 0.0;
            s.vel_h = 0.0;
            s.inertia_active = false;
            return;
        }

        let mut sv = if s.params.natural_scroll { -vel_v } else { vel_v };
        let mut sh = if s.params.h_enabled {
            if s.params.natural_scroll { -vel_h } else { vel_h }
        } else {
            0.0
        };

        sv = (sv * s.params.sensitivity).clamp(-s.params.max_speed, s.params.max_speed);
        sh = (sh * s.params.h_sensitivity).clamp(-s.params.max_speed, s.params.max_speed);

        s.vel_v = sv;
        s.vel_h = sh;
        s.inertia_active = true;
    }

    pub fn cancel_inertia(&self) {
        let mut s = self.state.lock().unwrap();
        s.vel_v = 0.0;
        s.vel_h = 0.0;
        s.inertia_active = false;
    }

    pub fn set_params(&self, p: ScrollParams) {
        let mut s = self.state.lock().unwrap();
        s.params = p;
    }
}

impl Drop for ScrollEngine {
    fn drop(&mut self) {
        if let Ok(mut s) = self.state.lock() {
            s.running = false;
        }
        if let Some(t) = self.tick_thread.take() {
            let _ = t.join();
        }
    }
}
