# Magic Mouse Driver for Windows

**Execution Plan · v4.1 · Production-Ready**

> Free · Open-Source · macOS-Quality Scrolling on Windows

---

## 1. Objective

Build a free, open-source alternative to Magic Mouse Utilities for Windows, delivering a macOS-quality experience with no licensing cost.

- Smooth scrolling with macOS-like inertia and pixel precision, including horizontal
- Full gesture support: swipes, taps, three-finger tap, double-tap
- Customisable controls via JSON config and GUI
- Stable Bluetooth performance across chipsets
- Lightweight background service with auto-start
- Signed installer — zero SmartScreen warnings

---

## 2. High-Level Strategy

The project avoids writing a kernel driver from scratch. Instead, it builds on Apple Boot Camp drivers as the HID base and focuses all innovation on the gesture and scrolling layers — where existing tools are weakest.

- Reuse Apple Boot Camp drivers as the HID base — they handle raw X/Y movement and clicks natively through Windows HID; the service does **not** re-inject pointer movement
- Custom gesture + scrolling engine runs entirely in userspace, supplementing only scroll and gesture events
- Generic HID fallback for machines without Boot Camp
- Windows background service for reliability and auto-start
- WinUI 3 frontend for configuration — optional, service works headlessly
- Signed installer built with WiX Toolset v4

---

## 3. Repository Structure

```
magic-mouse-win/
  core/
    hid_reader/          # HIDAPI wrapper
    device_detector/     # VID/PID detection logic
    input_parser/        # raw packet decoding
    gesture_engine/      # gesture state machine
    scroll_engine/       # inertia + smoothing (vertical + horizontal)
    config/              # JSON parsing + schema migration
  service/
    windows_service.cpp  # background service entry
    device_manager.cpp   # connect/disconnect/sleep-resume handling
    watchdog.cpp         # crash recovery + restart logic
    power_manager.cpp    # sleep/resume power event handling
  integration/
    input_injector.cpp   # SendInput wrapper (scroll + gestures only)
  ui/
    winui-app/           # C# WinUI 3 frontend
    tray_host/           # Win32 hidden HWND companion for system tray (H.NotifyIcon)
  tools/
    hid_logger/          # raw HID packet logger
    packet_visualizer/   # debug UI for touch data
  configs/
    default.json
  docs/
    architecture.md
    device-support.md
    packet-format.md     # reverse engineering notes
    troubleshooting.md
    contributing.md
  installer/
    wix/                 # WiX Toolset v4 config
    nsis/                # NSIS alternative
```

---

## 4. System Architecture

### 4.1 Driver Layer

Acquires raw HID input from the Magic Mouse using Apple Boot Camp drivers. All communication happens from userspace via HIDAPI — no kernel development required.

- Boot Camp driver mirror: [github.com/supermarsx/magicmouse](https://github.com/supermarsx/magicmouse)
- Alternative pack: [github.com/tealtadpole/MagicMouse2DriversWin11x64](https://github.com/tealtadpole/MagicMouse2DriversWin11x64)
- HIDAPI: [github.com/libusb/hidapi](https://github.com/libusb/hidapi)

> **Important:** The Boot Camp driver feeds raw X/Y pointer movement and button clicks into the standard Windows HID stack. Windows pointer ballistics ("Enhance pointer precision") are applied by the OS on this native path — the service does **not** intercept or re-inject pointer movement, preserving the native feel entirely.

> **Fallback:** Boot Camp drivers may not load on all hardware. On failure, the service auto-switches to Generic HID mode within 2 seconds, logs the fallback with adapter details, and shows `Limited mode active` in the tray icon.

---

### 4.2 Device Detection Layer

Enumerates devices via `hid_enumerate()` on startup and on every device-change notification, filtering by Apple Vendor ID `0x05AC`.

| Device        | VID    | PID    | Protocol           |
|---------------|--------|--------|--------------------|
| Magic Mouse 1 | 0x05AC | 0x030D | Bluetooth Classic  |
| Magic Mouse 2 | 0x05AC | 0x0269 | BLE                |

**Hot-plug Handling:**

- Register `DBT_DEVICEARRIVAL` and `DBT_DEVICEREMOVECOMPLETE` via `RegisterDeviceNotification`
- On arrival: re-enumerate, attempt connection, restore last config profile
- On removal: pause processing, release HID handle cleanly, enter waiting state
- Back-off retry on drop: `500ms → 1s → 2s → 5s max`

---

### 4.3 Battery Status

Battery level is read from a HID feature report on both Magic Mouse models and shown in the UI status bar.

- Read on connect, then polled every 60 seconds (avoids unnecessary BLE traffic)
- Decoded as a single byte: `0x00 = 0%`, `0x64 = 100%`
- Exact report ID and byte offset confirmed during Phase 0 HID logging
- Low battery desktop Toast notification at 15% (`Microsoft.Windows.AppNotifications`)
- If unavailable (Generic HID / old firmware): show `Battery: N/A` — never show stale data

```json
// IPC event emitted by service
{ "event": "battery", "device_serial": "XX:XX:XX:XX:XX:XX", "level": 82, "charging": false }
```

---

### 4.4 Multi-Device Handling

Defines clear, predictable behaviour when more than one Magic Mouse is present.

| Policy                       | Behaviour                                                              |
|------------------------------|------------------------------------------------------------------------|
| `first_connected` (default)  | Use whichever device connected first; ignore subsequent Magic Mice     |
| `preferred_serial`           | Always use the device matching `preferred_serial` in config            |
| `all`                        | Drive all connected devices simultaneously with the same config        |

- Default is `first_connected` — covers 99% of users with zero setup
- `preferred_serial`: user pins to a MAC address — shown in UI and copyable with one click
- Device selector dropdown appears in UI only when 2+ devices are present
- Switching active device writes `preferred_serial` to config and hot-reloads — no restart
- `all` policy: both devices share scroll and gesture config in v1.0; per-device config is Phase 5

> Tray notification when a second Magic Mouse is ignored: `'Second Magic Mouse detected — ignored (change in Settings)'`

---

### 4.5 Input Processing Layer

Converts raw HID packets into structured, typed input events using a two-thread pipeline.

> **Scope:** This layer reads and decodes **touch surface and button data only**. Raw X/Y pointer movement is handled by the Boot Camp driver through the native Windows HID stack — this pipeline does not touch it.

- Decodes packet structure (documented in `docs/packet-format.md` as reverse engineering progresses)
- Tracks finger positions across frames using nearest-neighbour assignment
- Filters noise: ignores contacts below minimum pressure threshold and edge-of-surface contacts
- Emits typed events: `FingerDownEvent`, `FingerMoveEvent`, `FingerUpEvent`, `ButtonEvent`

```
[HID Reader Thread]  →  lock-free ring buffer  →  [Processing Thread]
      60-250Hz                (~512 entries)              60-120Hz
```

> The HID Reader thread never blocks. The lock-free ring buffer between threads avoids mutex overhead and keeps polling latency tight.

---

### 4.6 Gesture Engine

Detects gestures from touch events using a state machine designed to eliminate the most common failure mode: a slow scroll that drifts horizontally near the end triggering an unintended swipe.

**Full State Model:**

```
IDLE
  └► TOUCH_START          (finger(s) down)
        └► TRACKING
              ├► SCROLL_LOCK          (vertical/horizontal movement dominant)
              │     └► END            (finger lift → inertia handoff to scroll engine)
              └► GESTURE_CANDIDATE   (movement matches gesture threshold)
                    ├► GESTURE_CONFIRMED   (threshold met cleanly)
                    │     └► END
                    └► GESTURE_AMBIGUOUS   (conflicts with scroll intent)
                          └► resolve by velocity vector → SCROLL_LOCK or GESTURE_CONFIRMED
```

**Scroll-to-Gesture Conflict Resolution** — four layers applied in order:

1. `SCROLL_LOCK` state: once entered, no transition to `GESTURE_CANDIDATE` is possible until all fingers lift
2. `gesture_lock_ms`: after fingers lift and re-touch, gestures are suppressed for this window (default 300ms). Prevents 'scroll then quick swipe' false positives
3. Velocity vector check: gesture confirmation requires horizontal velocity dominance at the moment of classification — not just at lift
4. User tuning: all three parameters exposed in UI

**Detection Parameters (all configurable):**

| Parameter                  | Default | Description                                    |
|----------------------------|---------|------------------------------------------------|
| `min_swipe_distance_px`    | 8px     | Minimum travel to register a swipe             |
| `max_swipe_time_ms`        | 400ms   | Gesture must complete within this window       |
| `scroll_lock_threshold_px` | 3px     | Vertical movement to enter SCROLL_LOCK         |
| `gesture_velocity_ratio`   | 2.0x    | Horizontal/vertical ratio to prefer gesture    |
| `double_tap_interval_ms`   | 250ms   | Max time between taps for double-tap           |
| `tap_max_movement_px`      | 3px     | Max drift allowed during a tap                 |
| `gesture_lock_ms`          | 300ms   | Gesture suppression window after scroll begins |

**Supported Gestures (MVP):**

| Gesture             | Default Action   | Notes                                                  |
|---------------------|-----------------|--------------------------------------------------------|
| Swipe left          | `ALT+LEFT`      | One finger                                             |
| Swipe right         | `ALT+RIGHT`     | One finger                                             |
| Swipe up            | `WIN+TAB`       | One finger                                             |
| Swipe down          | `WIN+D`         | One finger                                             |
| Single tap          | Left click      | —                                                      |
| Two-finger tap      | Right click     | —                                                      |
| Three-finger tap    | Middle click    | Preferred over double-tap; no false-positive risk      |
| Double tap          | Configurable    | Defaults to off; can be remapped by user if desired    |

> **Pinch-to-Zoom:** Intentionally deferred to Phase 5. The Magic Mouse touch surface is small and reports only 2 finger contact points, making reliable pinch detection on Windows much harder than on a trackpad. Phase 5 will evaluate using a trained classifier rather than heuristics. Until then, this gesture is explicitly out of scope and documented as such in `docs/troubleshooting.md`.

---

### 4.7 Scrolling Engine

The core differentiator of the project. Replicates macOS-style pixel-precise smooth scrolling on Windows (vertical and horizontal), which uses line-based scrolling by default.

**Physics Model (shared for both axes):**

```
On finger lift:
  initial_velocity = avg velocity over last 5 frames (pixels/ms)   // per axis

Each tick (at 120Hz):
  velocity *= decay_rate          // default: 0.92
  if abs(velocity) < 0.5: stop   // dead zone - prevents micro-drift
  position += velocity * delta_time
  inject scroll event for delta   // vertical or horizontal
```

#### Vertical Scroll Injection

- Use `SendInput()` with `MOUSEEVENTF_WHEEL`
- Accumulate fractional `WHEEL_DELTA` values; inject only when accumulator crosses a whole-unit threshold
- Achieves sub-line-height smoothness in Chromium, Firefox, and most Win32 apps

#### Horizontal Scroll Injection

- Use `SendInput()` with `MOUSEEVENTF_HWHEEL`
- Separate fractional accumulator for horizontal deltas — independent of vertical
- Activated by predominantly horizontal single-finger movement that enters `SCROLL_LOCK` on the horizontal axis
- **App compatibility note:** Many legacy Win32 apps do not respond to `WM_MOUSEHWHEEL`. This is a platform limitation and is documented in `docs/troubleshooting.md`. Chromium-based apps and modern Win32 handle it correctly.

**Per-App Scroll Behaviour:**

| App Type            | Vertical | Horizontal | Notes                                                |
|---------------------|----------|------------|------------------------------------------------------|
| Chromium-based      | Excellent | Excellent  | Handles fractional wheel events natively             |
| Firefox             | Good      | Good       | May need `mousewheel.min_line_scroll_amount` tuning  |
| Win32 apps (modern) | Moderate  | Moderate   | Line-based — smoothing is best-effort                |
| Win32 apps (legacy) | Moderate  | None       | Many legacy apps ignore `WM_MOUSEHWHEEL`             |
| Games / DirectInput | Limited   | Limited    | Synthetic input may be blocked — documented limitation |

**Configurable Parameters:**

| Parameter           | Default | Range       |
|---------------------|---------|-------------|
| Decay rate          | 0.92    | 0.80 – 0.99 |
| Sensitivity         | 1.2     | 0.5 – 3.0   |
| Max speed (px/tick) | 80      | 20 – 200    |
| Dead zone           | 0.5     | 0.1 – 2.0   |
| Natural scroll      | true    | on / off    |
| Inertia             | true    | on / off    |
| H-scroll enabled    | true    | on / off    |
| H-scroll sensitivity| 1.0     | 0.5 – 3.0   |

---

### 4.8 Windows Integration Layer

Translates processed **scroll and gesture events** into OS-level input using `SendInput()`. Raw pointer movement and clicks are handled natively by the Boot Camp driver — they are never routed through this layer.

- **Scroll injection:** `SendInput()` with `MOUSEEVENTF_WHEEL` (vertical) and `MOUSEEVENTF_HWHEEL` (horizontal); batched to reduce syscall overhead
- **Gesture actions:** `SendInput()` with `INPUT_KEYBOARD` for key sequences (`ALT+LEFT`, `WIN+TAB`, etc.) and `INPUT_MOUSE` for click simulation
- Core service runs as `LocalSystem`

**On low-level hooks (`SetWindowsHookEx`):**

> **Decision: hooks are not used in the service.** Due to Session 0 isolation (services run in Session 0, the interactive desktop is Session 1+), `WH_MOUSE_LL` hooks installed from a `LocalSystem` service cannot observe or intercept the interactive user's input. If interception is needed in future, hooks must run from the UI/tray process (which runs in the user's session). For v1.0, raw HID reads via HIDAPI — which work correctly from any session context — are sufficient and preferred.

- UI runs as the logged-in user and communicates with service via Named Pipe — no elevation prompt after install

> **Known Limitation:** Games and anti-cheat systems (Valorant, EasyAntiCheat) block `SendInput`. Documented in `docs/troubleshooting.md`. No workaround attempted.

---

### 4.9 Windows Service Layer

Runs the engine reliably in the background across reboots, user sessions, and system sleep/resume cycles.

| Property | Value                      |
|----------|----------------------------|
| Type     | `SERVICE_WIN32_OWN_PROCESS`|
| Start    | `SERVICE_AUTO_START`       |
| Account  | `LocalSystem`              |

**Crash Recovery — SCM Policy:**

```
First failure:   SC_ACTION_RESTART  (delay: 2 seconds)
Second failure:  SC_ACTION_RESTART  (delay: 5 seconds)
Third failure:   SC_ACTION_RESTART  (delay: 30 seconds)
Reset period:    86400 seconds (1 day)
```

Configured at install time via `ChangeServiceConfig2()` — no manual user action required.

**Internal Watchdog (`watchdog.cpp`):**

- Separate watchdog thread monitors HID reader and processing threads
- If either thread stalls for >5 seconds with a device connected, watchdog restarts only the affected pipeline — not the full service
- All crash events logged with stack trace before restart
- Watchdog health status visible in UI status bar

**Session Handling:**

- Handles `SERVICE_CONTROL_SESSIONCHANGE` to detect user logon/logoff
- Reloads per-user config on logon
- Pauses processing on logoff while keeping device handle open

**Power Management (`power_manager.cpp`):**

The service subscribes to system power events to handle Sleep, Hibernate, and Modern Standby correctly. Without this, HIDAPI handles become stale on resume and require a full watchdog-triggered restart.

```cpp
// Registered on service start
RegisterPowerSettingNotification(hService, &GUID_SUSPEND_RESUME_SETTING, ...);
```

| Power Event                  | Service Action                                                              |
|------------------------------|-----------------------------------------------------------------------------|
| `PBT_APMSUSPEND`             | Cleanly close all HIDAPI device handles; flush pending scroll state         |
| `PBT_APMRESUMEAUTOMATIC`     | Re-enumerate devices via `hid_enumerate()`; reconnect with back-off retry   |
| `PBT_APMRESUMESUSPEND`       | (Same as above, user-triggered wake) Show tray notification if reconnect fails |

> All three events are handled in `power_manager.cpp`. The sleep/resume path is covered by the same back-off retry logic as hot-plug: `500ms → 1s → 2s → 5s max`. Resume events go through the same re-enumeration path as `DBT_DEVICEARRIVAL`, so no duplicate code.

---

### 4.10 IPC Layer (UI ↔ Service)

Named Pipes provide low-overhead, local-only communication between the UI and the service.

- **Transport:** Windows Named Pipes (`\\.\pipe\MagicMouseService`)
- **Protocol:** newline-delimited JSON
- ACL restricted to current logged-in user and `LocalSystem` — no network exposure

```json
// UI → Service
{ "cmd": "get_status" }
{ "cmd": "reload_config" }
{ "cmd": "set_param", "key": "scroll.sensitivity", "value": 1.5 }

// Service → UI
{ "event": "status", "devices": [
    { "serial": "XX:XX:XX:XX:XX:XX", "model": "Magic Mouse 2", "mode": "bootcamp", "battery": 82, "active": true },
    { "serial": "YY:YY:YY:YY:YY:YY", "model": "Magic Mouse 2", "mode": "bootcamp", "battery": 61, "active": false }
  ] }
{ "event": "battery", "device_serial": "XX:XX", "level": 82, "charging": false }
{ "event": "log", "level": "INFO", "message": "Gesture: swipe_left" }
{ "event": "error", "code": "HID_OPEN_FAILED", "detail": "..." }
```

---

### 4.11 UI Layer

C# with WinUI 3 (Windows App SDK). UI is optional — the service works headlessly with `default.json`.

- Gesture mapping editor: dropdown per gesture mapped to action
- Scroll tuning sliders with live preview (sends `set_param` to service in real time) — covers both vertical and horizontal axes
- Profile management: global + per-app, stored as separate JSON files
- Device status bar: connected / limited mode / battery % with icon
- Multi-device selector dropdown (appears only when 2+ devices detected)
- Debug log viewer streaming from service via Named Pipe
- Startup toggle: enable/disable service auto-start
- First-run wizard guiding user through driver check and initial config
- **Tray icon:** implemented via [`H.NotifyIcon`](https://github.com/HavenDV/H.NotifyIcon) NuGet package (WinUI 3–compatible `NotifyIcon` library), since WinUI 3 / Windows App SDK has no native system tray API. Shows device status; right-click for quick settings or main window.

---

## 5. Dual Mode Support

| Feature              | Mode A: Boot Camp (Full) | Mode B: Generic HID (Fallback) |
|----------------------|--------------------------|--------------------------------|
| Full gesture support | Yes                      | No                             |
| Vertical scrolling   | Full inertia             | Basic                          |
| Horizontal scrolling | Full inertia             | Not available                  |
| Scroll + click       | Yes                      | Yes                            |
| Touch surface data   | Available                | Unavailable                    |
| Auto-selected when   | Boot Camp drivers loaded | Boot Camp absent or failed     |

> Mode is logged on startup and shown in the UI status bar. When Generic HID mode is active, the tray tooltip reads: `'Running in limited mode — install Boot Camp drivers for full features'`

---

## 6. Debugging Tools

### 6.1 HID Logger

- Standalone executable — works without the service running
- Dumps raw HID reports to a timestamped `.bin` file
- Outputs human-readable hex to stdout
- Used by contributors to capture packets from new hardware for reverse engineering

### 6.2 Packet Visualizer

- Reads `.bin` log files or a live device stream
- Plots touch coordinates on a 2D canvas in real time
- Highlights finger tracking assignments per frame
- Essential for validating `input_parser` output during development

---

## 7. Configuration

Config files are stored in `%LocalAppData%\MagicMouse\config\`

- `default.json`: shipped with installer, never modified by the service
- `user.json`: user overrides, merged over default at runtime
- `profiles\<AppName>.json`: per-app overrides matched by foreground window executable name

**Config Schema Migration:**

- `schema_version` is stored in **both** `user.json` and every `profiles\<AppName>.json`
- On startup, the service checks `schema_version` in each file independently
- If any file's `schema_version` is older than current:
  1. Back up the file with a `.bak` suffix (e.g., `user.json.bak`, `chrome.json.bak`)
  2. Run the appropriate migration transform
  3. Write the migrated file with updated `schema_version`
- Never silently overwrites any config file without a backup
- Migration failures are logged at `ERROR` level; the file is left as-is and default values are used

**Full Config Schema:**

```json
{
  "schema_version": 1,
  "gestures": {
    "swipe_left":        { "action": "SEND_KEYS", "keys": "ALT+LEFT" },
    "swipe_right":       { "action": "SEND_KEYS", "keys": "ALT+RIGHT" },
    "swipe_up":          { "action": "SEND_KEYS", "keys": "WIN+TAB" },
    "swipe_down":        { "action": "SEND_KEYS", "keys": "WIN+D" },
    "three_finger_tap":  { "action": "MIDDLE_CLICK" },
    "two_finger_tap":    { "action": "RIGHT_CLICK" },
    "double_tap":        { "action": "NONE" }
  },
  "scroll": {
    "natural": true,
    "inertia": true,
    "sensitivity": 1.2,
    "decay": 0.92,
    "max_speed": 80,
    "dead_zone": 0.5,
    "horizontal_enabled": true,
    "horizontal_sensitivity": 1.0
  },
  "gesture_thresholds": {
    "min_swipe_distance_px": 8,
    "max_swipe_time_ms": 400,
    "scroll_lock_threshold_px": 3,
    "gesture_velocity_ratio": 2.0,
    "double_tap_interval_ms": 250,
    "tap_max_movement_px": 3,
    "gesture_lock_ms": 300
  },
  "device": {
    "preferred_serial": "",
    "multi_device_policy": "first_connected"
  },
  "service": {
    "log_level": "INFO",
    "startup": "auto",
    "update_channel": "stable"
  }
}
```

---

## 8. Logging Strategy

| Level   | Usage |
|---------|-------|
| `ERROR` | Unrecoverable failures, crash events |
| `WARN`  | Recoverable issues, unexpected states |
| `INFO`  | Normal operational events (default in production) |
| `DEBUG` | Verbose trace — toggleable from UI without restart |

- **Location:** `%LocalAppData%\MagicMouse\logs\`
- **Rotation:** 5MB max per file, keep last 5 files
- **Format:** `magicmouse_YYYYMMDD_HHMMSS.log`
- All crash events include stack trace before service restarts
- Log viewer in UI streams live via Named Pipe

---

## 9. Installer Strategy

Primary: WiX Toolset v4. The install sequence is strictly ordered because the service must be registered after the Boot Camp driver step.

**Install Sequence:**

1. Check Windows version (minimum: Windows 10 21H2)
2. Check for existing installation — offer upgrade path
3. Install Boot Camp drivers (optional, user can skip)
4. Copy service binary to `C:\Program Files\MagicMouse\`
5. Copy `default.json` to `%LocalAppData%\MagicMouse\config\`
6. Register Windows Service (`sc create`)
7. Configure crash recovery policy (`ChangeServiceConfig2`)
8. Register auto-start
9. Install UI app (optional)
10. Start service
11. Add uninstall entry to Programs & Features

**Code Signing:**

- Installer `.msi` and service `.exe` signed with EV code signing certificate
- Eliminates SmartScreen `Unknown publisher` warning entirely
- For open-source dev builds without a cert: document test-signing mode in README and publish SHA256 checksums on GitHub Releases
- SmartScreen reputation builds over time with download volume — early release warning documented in README

**Silent Install:**

```
msiexec /i MagicMouse.msi /quiet ADDLOCAL=ALL
```

---

## 10. Versioning and Update Strategy

### 10.1 Versioning Scheme

Semantic Versioning: `MAJOR.MINOR.PATCH`

| Segment | Increments When |
|---------|----------------|
| MAJOR   | Breaking config changes, driver model changes |
| MINOR   | New features: gestures, UI additions, new device support |
| PATCH   | Bug fixes, stability improvements, no behaviour change |

Examples:
- `1.0.0` → initial public release
- `1.1.0` → per-app profiles added
- `1.1.1` → fix scroll dead zone regression
- `2.0.0` → breaking config schema change (migration tool provided)

### 10.2 Update Channels

| Channel            | Audience        | Cadence                             |
|--------------------|-----------------|-------------------------------------|
| `stable` (default) | All users       | Major/minor releases only           |
| `beta`             | Opt-in testers  | Release candidates and pre-releases |

### 10.3 Update Mechanism

- Service checks GitHub Releases API on startup and once per day
- Compares current version against `tag_name` in API response
- If update available: tray notification `'Update available: v1.2.0 — View release notes'`
- Clicking opens GitHub Releases page in browser — user downloads and installs manually in v1.0

**Auto-Update (Phase 5):**

- Background download of signed `.msi`
- SHA256 verification before install
- Prompt user to install — never silent, never forced
- Rollback: previous version installer retained in `%LocalAppData%\MagicMouse\updates\` for one version

> **Privacy:** No telemetry. No crash reports sent without explicit user opt-in. Stated clearly in README and first-run wizard.

---

## 11. Development Phases

### Phase 0 — Tools First *(1–2 weeks)*

- Build HID logger
- Capture raw packets from Magic Mouse 1 and 2
- Analyse and document packet format in `docs/packet-format.md`
- Confirm battery report ID and byte offset
- Build packet visualiser to validate findings

> **Exit criterion:** Touch coordinates, finger count, and battery level reliably decoded from raw packets.

### Phase 1 — MVP *(2–4 weeks)*

- Device detection and connection
- Basic movement and click passthrough (via Boot Camp driver native path)
- Single-finger vertical scroll (no inertia yet)
- CLI config file support
- Windows Service skeleton with crash recovery policy
- Basic power event handling (sleep/resume)

> **Exit criterion:** Mouse functions as a basic Windows mouse with vertical scroll.

### Phase 2 — Core Features *(3–6 weeks)*

- Full gesture state machine with `SCROLL_LOCK` / `GESTURE_CANDIDATE` split
- Smooth scrolling engine with inertia and `gesture_lock_ms` — both vertical and horizontal axes
- `MOUSEEVENTF_HWHEEL` horizontal scroll injection with independent accumulator
- Battery status reading and IPC event
- Multi-device policy enforcement
- Dual mode: Boot Camp + Generic HID fallback
- Full JSON config support with schema versioning (user.json and profiles)
- Named Pipe IPC
- Logging with rotation
- Full power management: `PBT_APMSUSPEND` / `PBT_APMRESUMEAUTOMATIC`

> **Exit criterion:** Usable as a daily driver without UI.

### Phase 3 — UI *(2–3 weeks)*

- WinUI 3 application
- Gesture mapping editor (including three-finger tap and configurable double-tap)
- Scroll tuning sliders with live preview — vertical and horizontal axes
- Device status bar with battery and multi-device selector
- Profile management
- Tray icon via `H.NotifyIcon` and first-run wizard

> **Exit criterion:** Non-technical user can install and configure without editing JSON.

### Phase 4 — Production Hardening *(2–3 weeks)*

- EV code signing certificate setup
- Signed WiX installer
- GitHub Actions CI: build, unit tests, static analysis on every PR
- 8-hour continuous stress testing sessions including sleep/resume cycles
- Full documentation: README, contributing guide, issue templates
- SmartScreen reputation building begins

> **Exit criterion:** Public v1.0 release on GitHub with signed installer.

### Phase 5 — Advanced Features *(Ongoing)*

- Per-application profiles with UI editor
- Advanced gesture threshold tuning UI
- Auto-update mechanism (GitHub Releases, signed `.msi`, SHA256 verification)
- Per-device config in multi-device mode
- Pinch-to-zoom evaluation (ML classifier approach)
- Plugin / extension API
- Community gesture packs

---

## 12. Key Challenges and Resolutions

### 1. Undocumented HID Packet Format

Apple does not publish touch surface packet structure for Windows.

- Phase 0 is dedicated entirely to this — build logger first, decode second
- Cross-reference with existing open-source reverse engineering in the community
- Document all findings in `docs/packet-format.md` — this becomes a community asset
- Packet visualiser accelerates pattern recognition significantly

### 2. Bluetooth Variability

Intel, Broadcom, and MediaTek BT adapters behave differently in latency, dropout, and reconnect behaviour.

- Test on minimum 3 chipsets during Phase 1: Intel AX210, Broadcom BCM20702, MediaTek MT7921
- Back-off retry logic on disconnect and resume: `500ms → 1s → 2s → 5s max`
- Community-updated compatibility matrix in `docs/device-support.md`
- Service logs adapter details on connect for diagnostics

### 3. Scroll Behaviour Differences Across Apps

Windows apps consume scroll events differently — Win32 uses line-based `WM_MOUSEWHEEL`, Chromium handles fractional deltas, some apps ignore `WM_MOUSEHWHEEL` for horizontal scroll, and some ignore synthetic input.

- Fractional `WHEEL_DELTA` and `HWHEEL_DELTA` accumulation handles most cases smoothly
- Per-app scroll profiles let users tune sensitivity per application
- Known-bad apps (especially those ignoring `WM_MOUSEHWHEEL`) documented in troubleshooting guide
- Games and anti-cheat apps explicitly listed as out of scope

### 4. Gesture Ambiguity

Small touch surface makes distinguishing scroll from swipe unreliable with simple threshold checks.

- Full state machine with `SCROLL_LOCK` / `GESTURE_CANDIDATE` split
- Velocity vector dominance resolution — not just distance
- `gesture_lock_ms` suppresses gestures immediately after scroll begins
- All thresholds exposed in UI for user tuning

### 5. Service Stability

A crash in the service leaves the user with a non-functional mouse.

- SCM crash recovery policy auto-restarts: `2s → 5s → 30s`
- Internal watchdog restarts only the affected pipeline thread, not the full service
- All crashes logged with stack trace before restart
- Watchdog health status visible in UI

### 6. Unsigned Installer / SmartScreen

Unsigned installers trigger SmartScreen warnings and erode user trust.

- EV code signing certificate for release builds — eliminates the warning
- SHA256 checksums published on GitHub Releases for open-source dev builds
- Early release warning documented in README while SmartScreen reputation builds
- Silent install path available for IT deployments

### 7. Sleep/Resume Reliability

On Modern Standby and traditional S3 sleep, Bluetooth stacks are torn down and HIDAPI handles become stale on wake.

- `power_manager.cpp` subscribes to `RegisterPowerSettingNotification`
- Handles `PBT_APMSUSPEND` (proactive handle close) and `PBT_APMRESUMEAUTOMATIC` (re-enumerate)
- Resume re-enumeration reuses the same back-off retry path as device hot-plug
- Stress test includes deliberate sleep/resume cycles throughout the 8-hour session

---

## 13. Testing Strategy

### Unit Tests *(automated, every commit)*

- Gesture engine: synthetic touch event sequences → assert correct gesture output
- Scroll engine: assert velocity decay curve, dead zone cutoff, natural scroll inversion
- **Horizontal scroll:** assert `HWHEEL_DELTA` accumulation and injection correctness
- Config parser: valid JSON, malformed JSON, missing keys, unknown keys, schema migration
- **Profile migration:** per-app profile JSON with old `schema_version` → assert upgraded correctly and `.bak` created
- Packet parser: known byte sequences → assert correct coordinate and button output
- Battery parser: known report bytes → assert correct percentage output

### Integration Tests *(automated, every commit)*

- HID simulation → gesture engine → injector pipeline (mocked `SendInput`)
- Service start / stop / restart cycle
- Named Pipe connect / disconnect / reconnect
- Config hot-reload via IPC
- Multi-device policy enforcement (simulated two-device arrival)
- **Sleep/resume simulation:** mock `PBT_APMSUSPEND` → assert handles closed; mock `PBT_APMRESUMEAUTOMATIC` → assert re-enumeration triggered

### Hardware Tests *(manual, per phase milestone)*

| Dimension   | Coverage                                        |
|-------------|--------------------------------------------------|
| OS          | Windows 10 (21H2), Windows 11 (latest)          |
| Devices     | Magic Mouse 1 and Magic Mouse 2                 |
| BT Adapters | Intel AX210, Broadcom BCM20702, MediaTek MT7921 |
| Modes       | Boot Camp mode and Generic HID mode             |
| Stress      | 8-hour continuous use sessions                  |
| Sleep/wake  | 20 deliberate sleep/resume cycles per session   |

### Behavioural Baseline

- Record macOS scroll curves (vertical and horizontal) using frame-by-frame screen capture
- Compare output curves from scroll engine against baseline at 120fps
- Target: visually indistinguishable in side-by-side comparison

### CI Pipeline *(GitHub Actions)*

```
On every PR:
  - Build (Windows MSVC + Clang)
  - Unit tests
  - Static analysis (clang-tidy, cppcheck)
  - Code formatting check

On merge to main:
  - All of the above
  - Integration tests
  - Signed installer build (release tags only)
```

---

## 14. Tech Stack

| Component            | Technology                                        |
|----------------------|---------------------------------------------------|
| Core Engine          | C++ (MSVC / Clang)                                |
| HID Access           | HIDAPI (libusb)                                   |
| UI                   | C# with WinUI 3 (Windows App SDK)                 |
| System Tray          | [`H.NotifyIcon`](https://github.com/HavenDV/H.NotifyIcon) NuGet package (WinUI 3 compatible) |
| IPC                  | Windows Named Pipes                               |
| Service              | Windows Service API                               |
| Power Events         | `RegisterPowerSettingNotification` (Win32)        |
| Config               | JSON (`nlohmann/json`)                            |
| Build                | CMake + vcpkg                                     |
| Installer            | WiX Toolset v4                                    |
| CI                   | GitHub Actions                                    |
| Code Signing         | EV Certificate (release builds)                   |

---

## 15. Timeline

| Phase                          | Duration  |
|--------------------------------|-----------|
| Phase 0 — Tools                | 1–2 weeks |
| Phase 1 — MVP                  | 2–4 weeks |
| Phase 2 — Core Features        | 3–6 weeks |
| Phase 3 — UI                   | 2–3 weeks |
| Phase 4 — Production Hardening | 2–3 weeks |
| Phase 5 — Advanced Features    | Ongoing   |

**Realistic total to v1.0: 12–18 weeks** (solo developer, part-time). The most variable factor is Phase 0 — HID packet decoding can be fast with community help or slow on undocumented firmware variants.

---

## 16. Success Criteria

| Metric                       | Target                                                     |
|------------------------------|------------------------------------------------------------|
| Scroll inertia (vertical)    | Visually matches macOS at 120fps in side-by-side test      |
| Scroll inertia (horizontal)  | Visually matches macOS at 120fps in side-by-side test      |
| Gesture accuracy             | >95% in normal use                                         |
| Input latency                | <5ms added over raw HID                                    |
| Gesture recognition latency  | <20ms from finger lift to action                           |
| Gesture false positives      | Zero swipe triggers during slow scroll in 30-min test      |
| Battery reporting            | Accurate within 5%, updates every 60s                      |
| Multi-device                 | Second device handled per policy within 2s of connect      |
| Installation time            | <2 minutes, no manual steps                                |
| SmartScreen warnings         | Zero (signed installer)                                    |
| Stability                    | Zero unrecovered crashes in 8-hour stress test             |
| Sleep/resume recovery        | Device reconnected within 10s of system wake in all tests  |
| Config migration             | Zero data loss on schema version upgrade (user.json + profiles) |
| Update notification          | Delivered within 24hrs of a new GitHub Release             |
| Community                    | 3+ active contributors within 6 months of v1.0            |

---

## 17. Future Ideas

- Machine learning gesture classifier to replace heuristics with a trained model
- Pinch-to-zoom using ML classifier (Phase 5 evaluation)
- Linux support via libinput
- Magic Trackpad support using the shared gesture engine
- Per-device config in multi-device mode (Phase 5)
- Community plugin marketplace
- Wayland / X11 support (long-term)

---

## 18. Summary

This project is technically feasible with a disciplined userspace-only approach. The architecture is solid — stay out of kernel mode entirely, build on Boot Camp drivers (letting them own pointer movement natively), and focus all innovation on the gesture and scrolling layers where existing tools are weakest.

**The critical path is:**

1. Decode the HID packet format in Phase 0 — everything depends on this
2. Nail the scrolling engine (vertical *and* horizontal) — this is the feature users will judge the project on
3. Get the service rock-solid before building the UI — a crashing service is worse than no UI
4. Ship a signed v1.0 with good defaults and let the community drive Phase 5

---

*v4.1 — Production-Ready Execution Plan · All architecture decisions finalised · All v4.0 concerns resolved · Ready for development start*

---

## 19. Architectural Concerns — Review Log

> This section tracks the concerns identified during the v4.0 architecture review. All concerns have been resolved and the fixes are integrated into the body of the document above.

---

### ✅ Concern 1: Cursor Movement & Pointer Ballistics — RESOLVED

**Original issue:** §4.8 implied `SendInput()` was used for all input including raw X/Y pointer movement, which would have bypassed Windows pointer ballistics.

**Resolution:** Architecture clarified throughout. The Boot Camp driver handles raw X/Y movement and clicks natively through the Windows HID stack — Windows pointer ballistics are applied by the OS and are never bypassed. The service uses `SendInput()` for **scroll and gesture events only**. See §2 (High-Level Strategy), §4.1 (Driver Layer note), §4.5 (Input Processing scope note), and §4.8 (Windows Integration Layer).

---

### ✅ Concern 2: Horizontal Scrolling Not Mentioned — RESOLVED

**Original issue:** The Scrolling Engine only described `MOUSEEVENTF_WHEEL` (vertical). Horizontal scrolling was completely absent.

**Resolution:** §4.7 now has a dedicated **Horizontal Scroll Injection** subsection covering `MOUSEEVENTF_HWHEEL`, its independent fractional accumulator, and app-compatibility notes (legacy Win32 apps that ignore `WM_MOUSEHWHEEL`). The config schema, per-app scroll table, configurable parameters, Phase 2 scope, Phase 3 UI scope, success criteria, and tech stack all reflect horizontal scrolling as a first-class feature.

---

### ✅ Concern 3: Sleep & Resume Not Handled — RESOLVED

**Original issue:** The plan covered hot-plug and logon/logoff but had no handling for Sleep/Hibernate, causing HIDAPI handles to go stale on wake.

**Resolution:** `power_manager.cpp` added to the repository structure and tech stack. §4.9 now includes a full **Power Management** subsection documenting `RegisterPowerSettingNotification`, handling of `PBT_APMSUSPEND` and `PBT_APMRESUMEAUTOMATIC`, and the fact that the resume path reuses the existing hot-plug back-off retry logic. Sleep/resume is added to Phase 1 (basic), Phase 2 (full), Phase 4 stress testing, and §13 integration tests. A new §12.7 challenge entry covers this explicitly.

---

### ✅ Concern 4: Pinch-to-Zoom & Three-Finger Gestures Absent — RESOLVED

**Original issue:** MVP gesture list omitted pinch-to-zoom entirely and used double-tap for middle click, which has high false-positive risk.

**Resolution:** (a) §4.6 now explicitly defers pinch-to-zoom to Phase 5 with a rationale (small surface, only 2 contact points on Windows, ML classifier approach preferred). (b) Default middle click binding changed to **three-finger tap** (reliable, no false positives). Double-tap is kept in the schema but defaults to `NONE` and is user-configurable. The gesture table, config schema, and Phase 3 UI section all reflect this.

---

### ✅ Concern 5: WinUI 3 Has No Native System Tray Support — RESOLVED

**Original issue:** WinUI 3 / Windows App SDK has no native `NotifyIcon` / system tray API. The plan promised a tray icon without addressing this.

**Resolution:** §4.11 explicitly states the tray icon is implemented via the `H.NotifyIcon` NuGet package (WinUI 3–compatible). `H.NotifyIcon` is added to §14 Tech Stack with a link to the library. The `tray_host/` directory is noted in §3 repository structure.

---

### ✅ Concern 6: Per-App Profile Config Migration Not Defined — RESOLVED

**Original issue:** `user.json` had clear migration logic, but `profiles\<AppName>.json` files had no `schema_version` or migration path.

**Resolution:** §7 now specifies that every per-app profile carries its own `schema_version`. The migration procedure (backup + migrate + update version) applies to each profile file independently. Failure handling (log at `ERROR`, fall back to defaults, leave file untouched) is documented. §13 unit tests now include a profile migration test case. Success criteria in §16 updated to cover profiles explicitly.

---

### ✅ Concern 7: Session 0 Isolation vs. WH_MOUSE_LL Hook — RESOLVED

**Original issue:** `SetWindowsHookEx (WH_MOUSE_LL)` was listed as the secondary input method, but hooks installed from a `LocalSystem` Session 0 service cannot observe interactive user-session input.

**Resolution:** §4.8 now explicitly documents the Session 0 isolation constraint and states that **`SetWindowsHookEx` is not used in the service**. Raw HID reads via HIDAPI (which work from any session context) are the correct and sufficient approach for v1.0. If hooks are ever needed in future, the document notes they must run from the UI/tray process in the user's session. The `hooks.cpp` file has been removed from the repository structure.
