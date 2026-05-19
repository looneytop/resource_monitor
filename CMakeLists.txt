cmake_minimum_required(VERSION 3.10)
project(ResourceMonitor)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(monitor src/main.cpp src/SystemCollector.cpp)