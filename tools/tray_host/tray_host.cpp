/**
 * tray_host.cpp — Magic Mouse Windows tray host
 *
 * Creates a system-tray icon.  Right-clicking it shows a context menu:
 *   Settings   – launches (or focuses) the WinUI settings window
 *   Pause      – sends pause command to the service via Named Pipe
 *   Exit       – removes the tray icon and quits
 *
 * The executable is built as a WIN32 subsystem binary (no console window).
 */

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>     // Shell_NotifyIcon, NOTIFYICONDATA
#include <strsafe.h>

// No longer using internal logger

// ── Constants ─────────────────────────────────────────────────────────────────

static constexpr UINT WM_TRAYICON   = WM_APP + 1;
static constexpr UINT IDI_TRAY      = 1;

static constexpr UINT IDM_SETTINGS  = 1001;
static constexpr UINT IDM_PAUSE     = 1002;
static constexpr UINT IDM_EXIT      = 1003;

static constexpr wchar_t kAppName[]        = L"Magic Mouse";
static constexpr wchar_t kWindowClass[]    = L"MagicMouseTrayHost";
static constexpr wchar_t kSettingsExe[]    = L"settings_app.exe";
static constexpr wchar_t kPipeName[]       = L"\\\\.\\pipe\\MagicMouseService";

// ── Globals ────────────────────────────────────────────────────────────────────

static HINSTANCE  g_hInst     = nullptr;
static HWND       g_hWnd      = nullptr;
static NOTIFYICONDATAW g_nid  = {};
static bool       g_paused    = false;

// ── IPC helper: send a UTF-8 JSON message to the service ─────────────────────

static bool SendPipeMessage(const char* json) {
    HANDLE pipe = CreateFileW(
        kPipeName,
        GENERIC_READ | GENERIC_WRITE,
        0, nullptr,
        OPEN_EXISTING, 0, nullptr);

    if (pipe == INVALID_HANDLE_VALUE) {
        // log warning
        return false;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

    DWORD written = 0;
    WriteFile(pipe, json, static_cast<DWORD>(strlen(json)), &written, nullptr);
    CloseHandle(pipe);
    return (written > 0);
}

// ── Tray icon management ──────────────────────────────────────────────────────

static void AddTrayIcon(HWND hWnd) {
    g_nid.cbSize           = sizeof(g_nid);
    g_nid.hWnd             = hWnd;
    g_nid.uID              = IDI_TRAY;
    g_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon            = LoadIconW(nullptr, IDI_APPLICATION);
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), kAppName);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}

static void RemoveTrayIcon() {
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
}

static void UpdateTrayTooltip(const wchar_t* tip) {
    StringCchCopyW(g_nid.szTip, ARRAYSIZE(g_nid.szTip), tip);
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// ── Context menu ─────────────────────────────────────────────────────────────

static void ShowContextMenu(HWND hWnd) {
    HMENU hMenu  = CreatePopupMenu();

    AppendMenuW(hMenu, MF_STRING, IDM_SETTINGS, L"Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_PAUSE,
                g_paused ? L"Resume" : L"Pause");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_EXIT,  L"Exit");

    // Required so the menu dismisses when the user clicks elsewhere.
    SetForegroundWindow(hWnd);

    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(hMenu,
                   TPM_BOTTOMALIGN | TPM_LEFTBUTTON,
                   pt.x, pt.y,
                   0, hWnd, nullptr);

    DestroyMenu(hMenu);
}

// ── Menu action handlers ──────────────────────────────────────────────────────

static void OnSettings() {
    // Try to launch the WinUI settings app side by side.
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);

    // Replace tray_host.exe with settings_app.exe in the same directory.
    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (lastSlash) {
        StringCchCopyW(lastSlash + 1,
                       MAX_PATH - (lastSlash - exePath) - 1,
                       kSettingsExe);
    }

    STARTUPINFOW        si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(exePath, nullptr, nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
        // Failed to launch
        MessageBoxW(g_hWnd,
                    L"Could not open the Settings window.\n"
                    L"Make sure settings_app.exe is in the same folder.",
                    kAppName, MB_ICONWARNING);
        return;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

static void OnPause() {
    const char* cmd = g_paused
        ? R"({"cmd":"resume"})"
        : R"({"cmd":"pause"})";

    if (SendPipeMessage(cmd)) {
        g_paused = !g_paused;
        UpdateTrayTooltip(g_paused ? L"Magic Mouse (paused)" : kAppName);
    } else {
        MessageBoxW(g_hWnd,
                    L"Could not reach the Magic Mouse service.\n"
                    L"Is it running?",
                    kAppName, MB_ICONWARNING);
    }
}

// ── Window procedure ─────────────────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            AddTrayIcon(hWnd);
            // Register class failed
            return 0;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_CONTEXTMENU) {
                ShowContextMenu(hWnd);
            } else if (lParam == WM_LBUTTONDBLCLK) {
                OnSettings();
            }
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_SETTINGS: OnSettings(); break;
                case IDM_PAUSE:    OnPause();    break;
                case IDM_EXIT:
                    RemoveTrayIcon();
                    PostQuitMessage(0);
                    break;
            }
            return 0;

        case WM_DESTROY:
            RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── WinMain ───────────────────────────────────────────────────────────────────

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) {
    g_hInst = hInst;

    // Register window class (invisible, message-only hybrid).
    WNDCLASSEXW wc     = {};
    wc.cbSize          = sizeof(wc);
    wc.lpfnWndProc     = WndProc;
    wc.hInstance       = hInst;
    wc.lpszClassName   = kWindowClass;
    RegisterClassExW(&wc);

    // Create a hidden window to receive messages.
    g_hWnd = CreateWindowExW(0, kWindowClass, kAppName, 0,
                              0, 0, 0, 0,
                              HWND_MESSAGE,   // message-only window
                              nullptr, hInst, nullptr);
    if (!g_hWnd) {
        g_hWnd = nullptr;
        PostQuitMessage(0);
        return 1;
    }

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        // Init componenttatic_cast<int>(msg.wParam);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
