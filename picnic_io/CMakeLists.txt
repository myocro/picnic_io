cmake_minimum_required(VERSION 3.5...3.31)
set (TARGET_NAME picnic_io)

project(${TARGET_NAME}
    VERSION 1.0.0
    DESCRIPTION "PICNIC I/O library"
    LANGUAGES CXX)
add_library(${TARGET_NAME} STATIC src/picnic_io.cpp)
target_compile_features   (${TARGET_NAME} PUBLIC cxx_std_17)
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include )
