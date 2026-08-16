cmake_minimum_required(VERSION 3.20)

# General
project(br_quartet_launcher)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
include(./cmake/cpm/CPM.cmake)

CPMAddPackage("gh:gabime/spdlog@1.17.0")

# Source
add_executable(br_quartet_launcher
    res/resources.rc
    src/main.cpp
    include/logger.hpp
    include/resources.h
)

target_include_directories(br_quartet_launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Linker
target_link_libraries(br_quartet_launcher PRIVATE
    spdlog::spdlog_header_only
)

if(MINGW)
    target_compile_options(br_quartet_launcher PRIVATE
        -ffunction-sections
        -fdata-sections
    )

    target_link_options(br_quartet_launcher PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
        "-Wl,--gc-sections"
    )

    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(br_quartet_launcher PRIVATE -s)
    endif()
endif()
