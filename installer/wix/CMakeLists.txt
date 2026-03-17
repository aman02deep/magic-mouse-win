# Find the WiX v4 CLI (dotnet tool install --global wix)
find_program(WIX_EXE wix)

if(WIX_EXE)
    set(INSTALLER_OUT "${CMAKE_BINARY_DIR}/MagicMouse-${PROJECT_VERSION}-Setup.msi")

    add_custom_target(installer
        COMMAND ${WIX_EXE} build
                "${CMAKE_CURRENT_SOURCE_DIR}/MagicMouse.wxs"
                -arch x64
                -d "BuildDir=${CMAKE_BINARY_DIR}"
                -d "SourceDir=${CMAKE_SOURCE_DIR}"
                -o "${INSTALLER_OUT}"
        COMMENT "Building WiX v4 installer → MagicMouse-${PROJECT_VERSION}-Setup.msi"
        VERBATIM
    )
else()
    message(STATUS "WiX CLI not found — installer target skipped. "
                   "Install with: dotnet tool install --global wix")
endif()
