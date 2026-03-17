add_library(scroll_engine STATIC
    scroll_engine.cpp
    scroll_engine.h
)

target_include_directories(scroll_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(scroll_engine PRIVATE cxx_std_17)
