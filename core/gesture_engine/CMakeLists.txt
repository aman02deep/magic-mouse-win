add_library(gesture_engine STATIC
    gesture_engine.cpp
    gesture_engine.h
)

target_include_directories(gesture_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(gesture_engine PRIVATE cxx_std_17)
