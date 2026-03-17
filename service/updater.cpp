#include "updater.h"
#include "logger.h"

#include <winhttp.h>
#include <shlobj.h>
#include <sstream>
#include <regex>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")

namespace mm {

namespace fs = std::filesystem;

// ── Version ─────────────────────────────────────────────────────────────────

Version Version::parse(const std::string& s) {
    Version v;
    // Accept "v1.2.3" or "1.2.3"
    std::regex re(R"(v?(\d+)\.(\d+)\.(\d+))");
    std::smatch m;
    if (std::regex_search(s, m, re)) {
        v.major = std::stoi(m[1]);
        v.minor = std::stoi(m[2]);
        v.patch = std::stoi(m[3]);
    }
    return v;
}

std::string Version::str() const {
    return std::to_string(major) + "." +
           std::to_string(minor) + "." +
           std::to_string(patch);
}

// ── Updater ─────────────────────────────────────────────────────────────────

Updater::Updater(const std::string& currentVersion,
                 const std::string& owner,
                 const std::string& repo,
                 UpdateReadyCallback onReady)
    : currentVersion_(currentVersion)
    , owner_(owner)
    , repo_(repo)
    , onReady_(std::move(onReady))
{
    stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
}

Updater::~Updater() {
    stop();
    if (stopEvent_) CloseHandle(stopEvent_);
}

void Updater::start() {
    thread_ = CreateThread(nullptr, 0,
        [](LPVOID param) -> DWORD {
            reinterpret_cast<Updater*>(param)->threadProc();
            return 0;
        }, this, 0, nullptr);
}

void Updater::stop() {
    if (stopEvent_) SetEvent(stopEvent_);
    if (thread_) {
        WaitForSingleObject(thread_, 5000);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
}

void Updater::threadProc() {
    LOG_INFO("updater", "starting; current version " + currentVersion_);

    // Check immediately on startup, then every 24 h.
    checkForUpdate();

    while (WaitForSingleObject(stopEvent_, kCheckIntervalMs) == WAIT_TIMEOUT) {
        checkForUpdate();
    }

    LOG_INFO("updater", "thread exiting");
}

// ── WinHTTP helpers ──────────────────────────────────────────────────────────

static std::string WinHttpGet(const std::wstring& host, const std::wstring& path) {
    HINTERNET hSession = WinHttpOpen(
        L"MagicMouse-Updater/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) return {};

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                        INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    // GitHub API requires User-Agent and Accept headers.
    WinHttpAddRequestHeaders(hRequest,
        L"Accept: application/vnd.github+json\r\n",
        (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD);

    std::string body;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                           WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
        && WinHttpReceiveResponse(hRequest, nullptr))
    {
        DWORD bytesAvail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &bytesAvail) && bytesAvail > 0) {
            std::string chunk(bytesAvail, '\0');
            DWORD bytesRead = 0;
            WinHttpReadData(hRequest, chunk.data(), bytesAvail, &bytesRead);
            body.append(chunk.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return body;
}

// ── Core logic ───────────────────────────────────────────────────────────────

std::string Updater::fetchLatestTag() {
    // GET https://api.github.com/repos/<owner>/<repo>/releases/latest
    std::wstring host = L"api.github.com";
    std::wstring path = L"/repos/" +
                        std::wstring(owner_.begin(), owner_.end()) + L"/" +
                        std::wstring(repo_.begin(), repo_.end()) +
                        L"/releases/latest";

    std::string json = WinHttpGet(host, path);
    if (json.empty()) return {};

    // Minimal parse: find "tag_name":"v0.2.0"
    std::regex re(R"("tag_name"\s*:\s*"([^"]+)")");
    std::smatch m;
    if (std::regex_search(json, m, re)) {
        return m[1].str();
    }
    return {};
}

std::string Updater::downloadMsi(const std::string& url, const std::string& tag) {
    // Build temp path: %TEMP%\MagicMouse-<tag>.msi
    wchar_t tempDir[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempDir);
    std::wstring wTag(tag.begin(), tag.end());
    std::wstring wPath = std::wstring(tempDir) + L"MagicMouse-" + wTag + L".msi";
    std::string  sPath(wPath.begin(), wPath.end());

    if (fs::exists(wPath)) return sPath; // Already downloaded.

    // Parse URL: expect https://github.com/...
    std::wstring host = L"github.com";
    std::wstring path(url.begin() + url.find("/repos") == std::string::npos
                       ? url.begin() + url.find(".com") + 4
                       : url.begin(), url.end());
    // Simplified: pass the full URL path portion after "github.com"
    auto pos = url.find("github.com");
    if (pos != std::string::npos) {
        path = std::wstring(url.begin() + pos + 10, url.end());
    }

    std::string body = WinHttpGet(host, path);
    if (body.empty()) {
        LOG_WARN("updater", "download failed: " + url);
        return {};
    }

    std::ofstream f(wPath, std::ios::binary);
    f.write(body.data(), static_cast<std::streamsize>(body.size()));
    return sPath;
}

bool Updater::checkForUpdate() {
    std::string tag = fetchLatestTag();
    if (tag.empty()) {
        LOG_WARN("updater", "could not fetch latest release tag");
        return false;
    }

    Version latest  = Version::parse(tag);
    Version current = Version::parse(currentVersion_);

    if (!(latest > current)) {
        LOG_INFO("updater", "up to date (" + currentVersion_ + ")");
        return false;
    }

    LOG_INFO("updater", "new version available: " + latest.str());

    // Build asset download URL (assumes standard release asset naming).
    std::string assetUrl = "https://github.com/" + owner_ + "/" + repo_ +
                           "/releases/download/" + tag +
                           "/MagicMouse-" + tag + "-Setup.msi";

    std::string msiPath = downloadMsi(assetUrl, tag);
    if (msiPath.empty()) return false;

    if (onReady_) {
        onReady_(latest, msiPath);
    } else {
        // Default: silent passive install.
        std::string cmd = "msiexec /passive /i \"" + msiPath + "\"";
        LOG_INFO("updater", "launching: " + cmd);
        system(cmd.c_str()); // NOLINT — acceptable here for installer launch
    }
    return true;
}

} // namespace mm
