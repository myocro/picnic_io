cmake_minimum_required(VERSION 3.10)

project(picnic_sample CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-g3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -s -DNDEBUG -march=native")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(picnic_io)

add_executable(picnic_cli picnic_cli_main.cc)
target_link_libraries(picnic_cli picnic_io)
