add_library(hid_reader STATIC
    hid_reader.cpp
    hid_reader.h
)

target_include_directories(hid_reader
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(hid_reader
    PUBLIC
        hidapi::hidapi
)
