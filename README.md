# Magic Mouse Utility (Go)

A lightweight, portable Windows utility to monitor your Apple Magic Mouse — battery level, device info, connection status — with a system tray icon and live web dashboard.

**No admin access required. No installation. Unzip and run.**

## Features

- 🖱 Detects Magic Mouse via BLE scanning (with PowerShell fallback)
- 🔋 Battery level (best-effort — Windows BT limitation)
- 📡 Live signal strength (RSSI)
- 🖥 Web dashboard at `http://localhost:7878` with live SSE updates
- 🔔 System tray icon with battery info (pure Go, no CGo)
- 🆔 Stable device ID from Bluetooth MAC address
- 📦 Portable single exe — no install, no registry, no admin

## Requirements

- Windows 10/11
- Go 1.22+ (for building — not needed to run)
- Apple Magic Mouse paired via Bluetooth

## Build & Run

```bat
git clone <repo>
cd magicmouse-util
build.bat
MagicMouseUtil.exe
```

### Command-Line Flags

| Flag | Default | Description |
|---|---|---|
| `--version` | | Print version and exit |
| `--no-tray` | `false` | Run without system tray (headless) |
| `--port` | `:7878` | Web dashboard port |
| `--no-browser` | `false` | Don't auto-open browser |

## Project Structure

```
magicmouse-util/
├── cmd/
│   └── main.go                  # Entry point
├── internal/
│   ├── device/
│   │   └── mouse.go             # Device model + ID generation
│   ├── bluetooth/
│   │   ├── monitor.go           # BLE scanning + PowerShell fallback
│   │   ├── battery_hid.go       # Battery reading (DEVPKEY, registry)
│   │   └── powershell.go        # PowerShell helpers
│   ├── server/
│   │   └── server.go            # HTTP + SSE server + embedded dashboard
│   └── tray/
│       └── tray.go              # Pure Go system tray (Shell_NotifyIconW)
├── build.bat
├── go.mod
└── README.md
```

## How It Works

1. **BLE Scan** — Uses `tinygo.org/x/bluetooth` to scan for Magic Mouse advertisements
2. **PowerShell Fallback** — Falls back to `Get-PnpDevice` if BLE adapter unavailable
3. **Battery** — Reads `DEVPKEY_Device_BatteryLevel` from HID nodes (best-effort)
4. **Dashboard** — Embedded HTML with Server-Sent Events for live updates
5. **Tray** — Pure Win32 `Shell_NotifyIconW` — no CGo, no external deps

## Battery Note

> Windows has limited Bluetooth battery reporting for Magic Mouse. If battery shows as "Unavailable", this is a Windows OS limitation, not a bug.

## Dependencies

| Package | Purpose |
|---|---|
| `tinygo.org/x/bluetooth` | BLE device scanning via WinRT |
| `golang.org/x/sys` | Win32 syscalls (tray, PowerShell) |

## License

MIT
