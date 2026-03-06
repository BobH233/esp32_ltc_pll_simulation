# ESP32-S3 LTC Timer Jitter Test
# 测试esp_timer的精度和抖动特性

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(timer_jitter_test)
