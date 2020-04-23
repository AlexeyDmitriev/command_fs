cmake_minimum_required(VERSION 3.16)
project(command_fs)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})


set(CMAKE_CXX_STANDARD 20)

find_package(FUSE REQUIRED)

add_executable(command_fs main.cpp)
target_link_libraries(command_fs fuse)
