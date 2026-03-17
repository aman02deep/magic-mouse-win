# Magic Mouse Windows Driver

A free, open-source alternative to Magic Mouse Utilities for Windows.
Delivers macOS-quality smooth scrolling and gesture support — no licensing cost.

## Features
- Smooth scrolling with macOS-like inertia (vertical + horizontal)
- Full gesture support: swipes, taps, three-finger tap
- Stable Bluetooth support across chipsets
- Lightweight background service with auto-start
- WinUI 3 configuration UI (optional)

## Build Requirements
- Windows 10 21H2 or later
- Visual Studio 2022 with "Desktop development with C++" workload
- CMake 3.20+
- vcpkg

## Building

```powershell
# 1. Clone vcpkg and integrate (one-time setup)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install

# 2. Configure
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# 3. Build
cmake --build build --config Debug
```

## Phase 0 — HID Logger

The first tool to build. Connect your Magic Mouse via Bluetooth, then run:

```powershell
.\build\tools\hid_logger\Debug\hid_logger.exe
```

The logger lists all detected Magic Mouse devices, lets you select one,
then dumps raw HID packets to stdout (hex) and to a timestamped `.bin` file.

Save the `.bin` file — it is used to reverse-engineer the packet format in Phase 1.

## Project Structure
```
core/           - Core engine libraries (hid_reader, device_detector, etc.)
service/        - Windows background service
integration/    - SendInput wrapper
ui/             - WinUI 3 frontend
tools/          - Developer tools (hid_logger, packet_visualizer)
configs/        - Default configuration JSON
docs/           - Architecture, packet format, troubleshooting
installer/      - WiX installer scripts
```

## License
MIT