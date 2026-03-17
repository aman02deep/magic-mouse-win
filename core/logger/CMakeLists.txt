add_library(logger STATIC
    logger.cpp
)

target_include_directories(logger PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(logger PRIVATE nlohmann_json::nlohmann_json)

set_target_properties(logger PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
