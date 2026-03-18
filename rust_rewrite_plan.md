# Magic Mouse Windows: Rust Rewrite Plan

This document outlines the architecture and migration strategy for rewriting the core background service of `MagicMouseService.exe` from C++ to **Rust**, while preserving the existing C# WPF Settings UI.

## Why Rust?
The current C++ implementation flawlessly handles low-level Windows APIs, but C++ builds rely heavily on `vcpkg` and `CMake` which cause deployment headaches and missing DLLs. A Rust rewrite provides:
* **Absolute Memory Safety:** No more silent background crashes or segmentation faults.
* **Zero-Cost Abstractions:** No garbage collector, ensuring `<1ms` latency for flawless mouse tracking and physics processing.
* **A Modern Build System:** `cargo` replaces `vcpkg` and `CMake`. Dependencies (like JSON parsing, named pipes, and Windows API wrappers) compile seamlessly into a single self-contained binary.

## Architecture

The project will remain a split architecture connected via Named Pipes (`\\.\pipe\MagicMouseService`).

### 1. The Core Driver: Rust (`magic_mouse_service`)
This component will replace the `service/` and `core/` C++ directories.
* **Language:** Rust (Edition 2021)
* **Windows APIs:** The `windows-rs` crate natively provided by Microsoft to interact with `SendInput` and the Service Control Manager.
* **Hardware Access:** The `hidapi` crate to read raw byte streams from the Bluetooth Apple Magic Mouse.
* **Responsibilities:**
  1. Boot up as a Windows Service (LocalSystem).
  2. Parse the `default.json` config.
  3. Start a Named Pipe listener (`tokio` or standard `std::os::windows::io`).
  4. Continuously poll the HID loop for raw mouse input.
  5. Apply physics (Momentum, Scroll speed, Swipe bounds) and inject `SendInput` events.

### 2. The Settings App & Tray: C# WPF (`settings_app` & `tray_host`)
The UI components remain completely identical.
* **Language:** C# (.NET 8.0 WPF)
* **Responsibilities:**
  1. `tray_host.exe` sits in the system tray and exposes a right-click menu.
  2. The WPF `settings_app.exe` provides a fully native Windows control panel.
  3. Both apps communicate with the Rust service over the same `\pipe\MagicMouseService` named pipe to pause the driver or notify it of `default.json` config changes.

## Migration Steps

1. **Initialize Cargo Workspace**
   ```bash
   cargo new magic_mouse_service --bin
   ```
2. **Setup Dependencies**
   Update `Cargo.toml` with:
   * `windows` (Microsoft's official Windows API wrapper for `SendInput` and `user32` calls)
   * `hidapi` (For hardware polling)
   * `serde` & `serde_json` (For parsing `configs/default.json`)
3. **Port Core Engines**
   * Port `core/scroll_engine.cpp` mathematics to Rust.
   * Port `core/gesture_engine.cpp` logic to Rust.
4. **Driver Loop**
   * Wrap the logic inside a Rust Windows Service scaffolding using the `windows-service` crate.
5. **Rip & Replace**
   * Delete `service/` and `core/` C++ files.
   * Update GitHub Actions `ci.yml` from `cmake` to `cargo build --release`.
   * Update WiX `MagicMouse.wxs` to point to the new Rust binary.
