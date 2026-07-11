cmake_minimum_required(VERSION 3.22.1)
project("gamebooster")

add_library(gamebooster SHARED DeviceOptimizer.cpp)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(gamebooster
    ${log-lib}
    ${android-lib})
