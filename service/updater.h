#pragma once

/**
 * updater.h — Magic Mouse auto-update via GitHub Releases API.
 *
 * Checked on service start-up and every 24 hours via a background thread.
 * If a newer version is found, the MSI is downloaded to %TEMP% and installed
 * silently using msiexec /passive.
 *
 * Uses WinHTTP only — no extra library dependency.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <functional>

namespace mm {

/// Semantic version (major.minor.patch).
struct Version {
    int major = 0, minor = 0, patch = 0;

    bool operator>(const Version& o) const {
        if (major != o.major) return major > o.major;
        if (minor != o.minor) return minor > o.minor;
        return patch > o.patch;
    }

    static Version parse(const std::string& s);
    std::string str() const;
};

/// Called asynchronously when an update is ready to install.
using UpdateReadyCallback = std::function<void(const Version& newVer, const std::string& msiPath)>;

class Updater {
public:
    /// @param currentVersion  The version this binary was built as (e.g. "0.1.0").
    /// @param owner           GitHub owner (e.g. "your-username").
    /// @param repo            GitHub repo name (e.g. "magic-mouse-win").
    /// @param onReady         Called on the updater thread when the MSI is ready.
    Updater(const std::string& currentVersion,
            const std::string& owner,
            const std::string& repo,
            UpdateReadyCallback onReady = nullptr);

    ~Updater();

    /// Starts the background polling thread.
    void start();

    /// Signals the background thread to stop (called on service shutdown).
    void stop();

private:
    void threadProc();
    bool checkForUpdate();            // Returns true if a newer release was found.
    std::string fetchLatestTag();     // HTTP GET to GitHub API.
    std::string downloadMsi(const std::string& url, const std::string& tag);

    std::string         currentVersion_;
    std::string         owner_;
    std::string         repo_;
    UpdateReadyCallback onReady_;

    HANDLE             thread_     = nullptr;
    HANDLE             stopEvent_  = nullptr;
    static constexpr DWORD kCheckIntervalMs = 24UL * 60 * 60 * 1000; // 24 h
};

} // namespace mm
