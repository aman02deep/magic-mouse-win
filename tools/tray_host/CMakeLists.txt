add_executable(tray_host WIN32
    tray_host.cpp
)

target_link_libraries(tray_host PRIVATE
    shell32      # Shell_NotifyIcon
    user32       # CreatePopupMenu, TrackPopupMenu, MessageBox
    comctl32     # Common controls (LoadIconMetric)
)



set_target_properties(tray_host PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    WIN32_EXECUTABLE TRUE
)

target_compile_definitions(tray_host PRIVATE
    UNICODE _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)
