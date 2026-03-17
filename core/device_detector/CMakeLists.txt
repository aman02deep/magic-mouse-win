add_library(device_detector STATIC
    device_detector.cpp
    device_detector.h
)

target_include_directories(device_detector
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(device_detector
    PUBLIC
        hidapi::hidapi
)
