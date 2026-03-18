use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};

pub const CURRENT_SCHEMA_VERSION: i32 = 1;

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct GestureConfig {
    #[serde(default = "default_swipe_left")]
    pub swipe_left: String,
    #[serde(default = "default_swipe_right")]
    pub swipe_right: String,
    #[serde(default = "default_swipe_up")]
    pub swipe_up: String,
    #[serde(default = "default_swipe_down")]
    pub swipe_down: String,
    #[serde(default = "default_three_finger_tap")]
    pub three_finger_tap: String,
    #[serde(default = "default_two_finger_tap")]
    pub two_finger_tap: String,
    #[serde(default = "default_double_tap")]
    pub double_tap: String,
}

fn default_swipe_left() -> String { "SEND_KEYS:ALT+LEFT".to_string() }
fn default_swipe_right() -> String { "SEND_KEYS:ALT+RIGHT".to_string() }
fn default_swipe_up() -> String { "SEND_KEYS:WIN+TAB".to_string() }
fn default_swipe_down() -> String { "SEND_KEYS:WIN+D".to_string() }
fn default_three_finger_tap() -> String { "MIDDLE_CLICK".to_string() }
fn default_two_finger_tap() -> String { "RIGHT_CLICK".to_string() }
fn default_double_tap() -> String { "NONE".to_string() }

impl Default for GestureConfig {
    fn default() -> Self {
        Self {
            swipe_left: default_swipe_left(),
            swipe_right: default_swipe_right(),
            swipe_up: default_swipe_up(),
            swipe_down: default_swipe_down(),
            three_finger_tap: default_three_finger_tap(),
            two_finger_tap: default_two_finger_tap(),
            double_tap: default_double_tap(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ScrollConfig {
    #[serde(default = "default_true")]
    pub natural: bool,
    #[serde(default = "default_true")]
    pub inertia: bool,
    #[serde(default = "default_sens")]
    pub sensitivity: f32,
    #[serde(default = "default_decay")]
    pub decay: f32,
    #[serde(default = "default_max_speed")]
    pub max_speed: f32,
    #[serde(default = "default_dead_zone")]
    pub dead_zone: f32,
    #[serde(default = "default_true")]
    pub horizontal_enabled: bool,
    #[serde(default = "default_one")]
    pub horizontal_sensitivity: f32,
}

fn default_true() -> bool { true }
fn default_sens() -> f32 { 1.2 }
fn default_decay() -> f32 { 0.92 }
fn default_max_speed() -> f32 { 80.0 }
fn default_dead_zone() -> f32 { 0.5 }
fn default_one() -> f32 { 1.0 }

impl Default for ScrollConfig {
    fn default() -> Self {
        Self {
            natural: true,
            inertia: true,
            sensitivity: 1.2,
            decay: 0.92,
            max_speed: 80.0,
            dead_zone: 0.5,
            horizontal_enabled: true,
            horizontal_sensitivity: 1.0,
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct GestureThresholdConfig {
    #[serde(default = "default_min_swipe_dist")]
    pub min_swipe_distance_px: i32,
    #[serde(default = "default_max_swipe_time")]
    pub max_swipe_time_ms: i32,
    #[serde(default = "default_scroll_lock")]
    pub scroll_lock_threshold_px: i32,
    #[serde(default = "default_gesture_vel")]
    pub gesture_velocity_ratio: f32,
    #[serde(default = "default_double_tap_int")]
    pub double_tap_interval_ms: i32,
    #[serde(default = "default_tap_max_mov")]
    pub tap_max_movement_px: i32,
    #[serde(default = "default_gesture_lock")]
    pub gesture_lock_ms: i32,
}

fn default_min_swipe_dist() -> i32 { 8 }
fn default_max_swipe_time() -> i32 { 400 }
fn default_scroll_lock() -> i32 { 3 }
fn default_gesture_vel() -> f32 { 2.0 }
fn default_double_tap_int() -> i32 { 250 }
fn default_tap_max_mov() -> i32 { 3 }
fn default_gesture_lock() -> i32 { 300 }

impl Default for GestureThresholdConfig {
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

#[derive(Debug, Serialize, Deserialize, Clone, Default)]
pub struct DeviceConfig {
    #[serde(default)]
    pub preferred_serial: String,
    #[serde(default = "default_multi_device")]
    pub multi_device_policy: String,
}
fn default_multi_device() -> String { "first_connected".to_string() }

#[derive(Debug, Serialize, Deserialize, Clone)]
pub struct ServiceConfig {
    #[serde(default = "default_log_level")]
    pub log_level: String,
    #[serde(default = "default_startup")]
    pub startup: String,
    #[serde(default = "default_update_channel")]
    pub update_channel: String,
}
fn default_log_level() -> String { "INFO".to_string() }
fn default_startup() -> String { "auto".to_string() }
fn default_update_channel() -> String { "stable".to_string() }
impl Default for ServiceConfig {
    fn default() -> Self {
        Self {
            log_level: "INFO".to_string(),
            startup: "auto".to_string(),
            update_channel: "stable".to_string(),
        }
    }
}

#[derive(Debug, Serialize, Deserialize, Clone, Default)]
pub struct AppConfig {
    #[serde(default = "default_schema_version")]
    pub schema_version: i32,
    #[serde(default)]
    pub gestures: GestureConfig,
    #[serde(default)]
    pub scroll: ScrollConfig,
    #[serde(default)]
    pub gesture_thresholds: GestureThresholdConfig,
    #[serde(default)]
    pub device: DeviceConfig,
    #[serde(default)]
    pub service: ServiceConfig,
    #[serde(default = "default_true")]
    pub enabled: bool,
    #[serde(default = "default_true")]
    pub run_at_startup: bool,
}
fn default_schema_version() -> i32 { CURRENT_SCHEMA_VERSION }

pub struct ConfigManager {
    pub config: AppConfig,
    config_dir: PathBuf,
}

impl ConfigManager {
    pub fn new() -> Self {
        let app_data = std::env::var("APPDATA").unwrap_or_else(|_| "C:\\".to_string());
        let config_dir = Path::new(&app_data).join("MagicMouse").join("configs");

        Self {
            config: AppConfig::default(),
            config_dir,
        }
    }

    pub fn load(&mut self) -> bool {
        let default_json_path = self.config_dir.join("default.json");
        if default_json_path.exists() {
            if let Ok(text) = fs::read_to_string(&default_json_path) {
                if let Ok(parsed) = serde_json::from_str::<AppConfig>(&text) {
                    self.config = parsed;
                    return true;
                }
            }
        }
        false
    }
}
