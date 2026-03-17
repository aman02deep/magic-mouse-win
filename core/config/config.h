/**
 * config.h — Phase 2
 *
 * JSON config loading, merging (default + user override), schema migration,
 * and per-app profile support. Uses nlohmann/json.
 */
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <optional>
#include <filesystem>

namespace magicmouse {

/// Current config schema version — increment on breaking changes
static constexpr int kCurrentSchemaVersion = 1;

struct GestureConfig {
    std::string swipe_left       = "SEND_KEYS:ALT+LEFT";
    std::string swipe_right      = "SEND_KEYS:ALT+RIGHT";
    std::string swipe_up         = "SEND_KEYS:WIN+TAB";
    std::string swipe_down       = "SEND_KEYS:WIN+D";
    std::string three_finger_tap = "MIDDLE_CLICK";
    std::string two_finger_tap   = "RIGHT_CLICK";
    std::string double_tap       = "NONE";
};

struct ScrollConfig {
    bool  natural              = true;
    bool  inertia              = true;
    float sensitivity          = 1.2f;
    float decay                = 0.92f;
    float max_speed            = 80.0f;
    float dead_zone            = 0.5f;
    bool  horizontal_enabled   = true;
    float horizontal_sensitivity = 1.0f;
};

struct GestureThresholdConfig {
    int   min_swipe_distance_px    = 8;
    int   max_swipe_time_ms        = 400;
    int   scroll_lock_threshold_px = 3;
    float gesture_velocity_ratio   = 2.0f;
    int   double_tap_interval_ms   = 250;
    int   tap_max_movement_px      = 3;
    int   gesture_lock_ms          = 300;
};

struct DeviceConfig {
    std::string preferred_serial    = "";
    std::string multi_device_policy = "first_connected";
};

struct ServiceConfig {
    std::string log_level       = "INFO";
    std::string startup         = "auto";
    std::string update_channel  = "stable";
};

struct AppConfig {
    int                    schema_version = kCurrentSchemaVersion;
    GestureConfig          gestures;
    ScrollConfig           scroll;
    GestureThresholdConfig gesture_thresholds;
    DeviceConfig           device;
    ServiceConfig          service;
};

/// Loads and merges default.json + user.json (+ optional per-app profile).
/// Handles schema migration and backs up old files before upgrading.
class ConfigManager {
public:
    ConfigManager();

    /// Load config from standard paths under %LocalAppData%\MagicMouse\config\
    bool load();

    /// Load per-app profile overlay (matched by foreground exe name)
    void applyProfile(const std::string& exe_name);

    /// Hot-reload user.json (called on IPC reload_config command)
    bool reload();

    /// Returns the merged, active config
    const AppConfig& get() const { return config_; }

    /// Persist a single key-value change from UI (e.g. set_param)
    bool setParam(const std::string& key_path, const nlohmann::json& value);

private:
    nlohmann::json loadJsonFile(const std::filesystem::path& path);
    bool migrateIfNeeded(const std::filesystem::path& path, nlohmann::json& j);
    void backupFile(const std::filesystem::path& path);

    AppConfig config_;
    std::filesystem::path config_dir_;
    nlohmann::json default_json_;
    nlohmann::json user_json_;
};

} // namespace magicmouse
