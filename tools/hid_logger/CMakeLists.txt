add_executable(hid_logger
    main.cpp
)

target_link_libraries(hid_logger
    PRIVATE
        hid_reader
        device_detector
)

target_compile_features(hid_logger PRIVATE cxx_std_17)
