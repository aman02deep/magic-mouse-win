cmake_minimum_required(VERSION 3.20)

# Default to static linking on Windows to avoid missing DLLs (hidapi, MSVCRT) in the installer
if(WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "vcpkg triplet")
endif()

project(MagicMouse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# vcpkg toolchain is passed via -DCMAKE_TOOLCHAIN_FILE
find_package(hidapi CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ── Phase 0: Core HID libraries ──────────────────────────────────────────────
add_subdirectory(core/hid_reader)
add_subdirectory(core/device_detector)

# ── Phase 0: Developer tools ──────────────────────────────────────────────────
add_subdirectory(tools/hid_logger)

# ── Phase 1/2: Engine libraries ───────────────────────────────────────────────
add_subdirectory(core/logger)
add_subdirectory(core/gesture_engine)
add_subdirectory(core/scroll_engine)
add_subdirectory(core/config)
add_subdirectory(integration)

# ── Phase 1/2: Windows Service ────────────────────────────────────────────────
add_subdirectory(service)

# ── Phase 3: Tray host + Settings UI ─────────────────────────────────────────
add_subdirectory(tools/tray_host)

# Build the WinUI 3 settings app via dotnet (requires Windows App SDK)
find_program(DOTNET_EXE dotnet)
if(DOTNET_EXE)
    add_custom_target(settings_app ALL
        COMMAND ${DOTNET_EXE} build ${CMAKE_SOURCE_DIR}/ui/settings_app/SettingsApp.csproj
                --configuration $<CONFIG>
                --output ${CMAKE_BINARY_DIR}/ui/settings_app
        COMMENT "Building WinUI 3 settings app"
    )
endif()

# ── Phase 4/5: Hardening & Advanced ─────────────────────────────────────────
add_subdirectory(core/plugin_api)
add_subdirectory(installer/wix)
