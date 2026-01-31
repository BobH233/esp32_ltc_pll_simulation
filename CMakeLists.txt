cmake_minimum_required(VERSION 3.10)
project(LTC_PLL_Simulator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用优化和警告
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# 源文件
set(SOURCES
    src/main.cpp
    src/clock_simulator.cpp
    src/pll_controller.cpp
    src/ltc_encoder.cpp
    src/ltc_decoder.cpp
    src/statistics.cpp
)

# 可执行文件
add_executable(ltc_simulator ${SOURCES})

# 链接数学库
target_link_libraries(ltc_simulator m)
