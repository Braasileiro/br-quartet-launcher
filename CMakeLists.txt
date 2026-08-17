cmake_minimum_required(VERSION 3.20)

# General
project(br_quartet_launcher)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies
include(./cmake/cpm/CPM.cmake)

CPMAddPackage("gh:gabime/spdlog@1.17.0")

# Source
add_executable(br_quartet_launcher WIN32
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

# Executable name
set_target_properties(br_quartet_launcher PROPERTIES
    OUTPUT_NAME "BLUE REFLECTION Quartet"
)

if(MINGW)
    target_compile_options(br_quartet_launcher PRIVATE
        -ffunction-sections
        -fdata-sections
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )

    target_link_options(br_quartet_launcher PRIVATE
        -static
        -static-libgcc
        -static-libstdc++
        "LINKER:--gc-sections"
    )

    target_link_options(br_quartet_launcher PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:-s>
    )
endif()
