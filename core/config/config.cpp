/**
 * config.cpp — Phase 2
 */
#include "config.h"
#include <fstream>
#include <iostream>
#include <cstdlib>

namespace magicmouse {

namespace {
    std::filesystem::path getConfigDir() {
        const char* appdata = std::getenv("LOCALAPPDATA");
        if (!appdata) appdata = ".";
        return std::filesystem::path(appdata) / "MagicMouse" / "config";
    }

    void fromJson(const nlohmann::json& j, ScrollConfig& s) {
        if (j.contains("natural"))               s.natural              = j["natural"];
        if (j.contains("inertia"))               s.inertia              = j["inertia"];
        if (j.contains("sensitivity"))           s.sensitivity          = j["sensitivity"];
        if (j.contains("decay"))                 s.decay                = j["decay"];
        if (j.contains("max_speed"))             s.max_speed            = j["max_speed"];
        if (j.contains("dead_zone"))             s.dead_zone            = j["dead_zone"];
        if (j.contains("horizontal_enabled"))    s.horizontal_enabled   = j["horizontal_enabled"];
        if (j.contains("horizontal_sensitivity")) s.horizontal_sensitivity = j["horizontal_sensitivity"];
    }
} // anonymous namespace

ConfigManager::ConfigManager()
    : config_dir_(getConfigDir()) {}

bool ConfigManager::load() {
    std::filesystem::create_directories(config_dir_);

    // 1. Load default.json (bundled, never modified)
    auto default_path = config_dir_ / "default.json";
    default_json_ = loadJsonFile(default_path);

    // 2. Load user.json (user overrides)
    auto user_path = config_dir_ / "user.json";
    if (std::filesystem::exists(user_path)) {
        user_json_ = loadJsonFile(user_path);
        // Migrate if schema version is older
        if (!migrateIfNeeded(user_path, user_json_)) {
            std::cerr << "[Config] Migration failed for user.json — using defaults.\n";
            user_json_ = {};
        }
    }

    // 3. Merge: default <- user overrides
    nlohmann::json merged = default_json_;
    if (!user_json_.is_null()) {
        merged.merge_patch(user_json_);
    }

    // 4. Populate typed config struct
    if (merged.contains("scroll")) fromJson(merged["scroll"], config_.scroll);
    // TODO: parse gestures, thresholds, device, service into structs

    return true;
}

void ConfigManager::applyProfile(const std::string& exe_name) {
    auto profile_path = config_dir_ / "profiles" / (exe_name + ".json");
    if (!std::filesystem::exists(profile_path)) return;

    auto pj = loadJsonFile(profile_path);
    if (!migrateIfNeeded(profile_path, pj)) {
        std::cerr << "[Config] Profile migration failed for " << exe_name << " — skipping.\n";
        return;
    }
    // Apply overlay (per-app overrides only scroll/gesture sections)
    if (pj.contains("scroll")) fromJson(pj["scroll"], config_.scroll);
}

bool ConfigManager::reload() {
    return load();
}

bool ConfigManager::setParam(const std::string& key_path, const nlohmann::json& value) {
    // Simple dot-separated key path  e.g. "scroll.sensitivity"
    // TODO: implement key-path writing to user.json
    (void)key_path; (void)value;
    return false;
}

nlohmann::json ConfigManager::loadJsonFile(const std::filesystem::path& path) {
    std::ifstream f(path);
    if (!f.is_open()) return {};
    try {
        return nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << "[Config] Parse error in " << path << ": " << e.what() << "\n";
        return {};
    }
}

bool ConfigManager::migrateIfNeeded(const std::filesystem::path& path, nlohmann::json& j) {
    if (j.is_null()) return true;
    int ver = j.value("schema_version", 0);
    if (ver == kCurrentSchemaVersion) return true;
    if (ver > kCurrentSchemaVersion) {
        std::cerr << "[Config] File schema " << ver << " is newer than app supports ("
                  << kCurrentSchemaVersion << "). Skipping.\n";
        return false;
    }
    // Back up old file before migrating
    backupFile(path);
    // Apply migrations from ver → kCurrentSchemaVersion
    // (Add transform steps here as the schema evolves)
    j["schema_version"] = kCurrentSchemaVersion;
    std::ofstream out(path);
    if (out.is_open()) out << j.dump(2);
    return true;
}

void ConfigManager::backupFile(const std::filesystem::path& path) {
    auto bak = path;
    bak.replace_extension(path.extension().string() + ".bak");
    std::filesystem::copy_file(path, bak, std::filesystem::copy_options::overwrite_existing);
    std::cout << "[Config] Backed up " << path.filename() << " → " << bak.filename() << "\n";
}

} // namespace magicmouse
