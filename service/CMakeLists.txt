add_executable(MagicMouseService
    windows_service.cpp
    device_manager.cpp
    device_manager.h
    power_manager.cpp
    power_manager.h
    watchdog.cpp
    watchdog.h
    ipc_server.cpp
    ipc_server.h
)

target_include_directories(MagicMouseService
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/core/hid_reader
        ${CMAKE_SOURCE_DIR}/core/device_detector
        ${CMAKE_SOURCE_DIR}/core/gesture_engine
        ${CMAKE_SOURCE_DIR}/core/scroll_engine
        ${CMAKE_SOURCE_DIR}/core/config
        ${CMAKE_SOURCE_DIR}/integration
)

target_link_libraries(MagicMouseService
    PRIVATE
        hid_reader
        device_detector
        gesture_engine
        scroll_engine
        config_manager
        input_injector
        hidapi::hidapi
        nlohmann_json::nlohmann_json
)

target_compile_features(MagicMouseService PRIVATE cxx_std_17)

# Windows subsystem: console for debug, windows for release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(MagicMouseService PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/SUBSYSTEM:WINDOWS"
    )
endif()
