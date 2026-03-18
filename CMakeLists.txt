cmake_minimum_required(VERSION 3.20)

project(MagicMouse VERSION 0.1.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Phase 1: Tray host (C++) ─────────────────────────────────────────────────
add_subdirectory(tools/tray_host)

# ── Phase 2: Settings UI (WPF / .NET) ─────────────────────────────────────────
find_program(DOTNET_EXE dotnet)
if(DOTNET_EXE)
    add_custom_target(settings_app ALL
        COMMAND ${DOTNET_EXE} publish ${CMAKE_SOURCE_DIR}/ui/settings_app/SettingsApp.csproj
                --configuration $<CONFIG>
                --runtime win-x64
                --self-contained true
                --output ${CMAKE_BINARY_DIR}/ui/settings_app
        COMMENT "Publishing WPF settings app (self-contained x64)"
    )
endif()

# ── Phase 3: Core Driver (Rust) ────────────────────────────────────────────────
find_program(CARGO_EXE cargo)
if(CARGO_EXE)
    add_custom_target(magic_mouse_service ALL
        COMMAND ${CARGO_EXE} build --release
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/service_rust
        COMMENT "Building Rust magic_mouse_service"
    )
    
    # Copy the compiled Rust executable to the expected output dir for Wix
    # Wix expects: $(var.BuildDir)\service\Release\MagicMouseService.exe
    add_custom_command(TARGET magic_mouse_service POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/service/Release
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_SOURCE_DIR}/service_rust/target/release/magic_mouse_service.exe
            ${CMAKE_BINARY_DIR}/service/Release/MagicMouseService.exe
    )
else()
    message(WARNING "Cargo not found. The Rust background service will not be built locally.")
endif()

# ── Phase 4: Installer ──────────────────────────────────────────────────────
add_subdirectory(installer/wix)
