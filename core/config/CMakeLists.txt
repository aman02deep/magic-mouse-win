add_library(config_manager STATIC
    config.cpp
    config.h
)

target_include_directories(config_manager PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(config_manager
    PUBLIC
        nlohmann_json::nlohmann_json
)

target_compile_features(config_manager PRIVATE cxx_std_17)
