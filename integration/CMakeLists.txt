add_library(input_injector STATIC
    input_injector.cpp
    input_injector.h
)

target_include_directories(input_injector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(input_injector PRIVATE cxx_std_17)
