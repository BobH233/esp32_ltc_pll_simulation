idf_component_register(
    SRCS "timer_jitter_test.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer
)

# 强制使用C++17
set_source_files_properties(
    timer_jitter_test.cpp
    PROPERTIES COMPILE_FLAGS "-std=gnu++17"
)
