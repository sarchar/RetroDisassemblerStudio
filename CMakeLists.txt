# Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
# All rights reserved.
# 
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree.  
cmake_minimum_required(VERSION 3.20)

find_package(OpenGL REQUIRED)

project(RetroDisassemblerStudio VERSION 0.0.2)

if(CMAKE_CONFIGURATION_TYPES)
    list(APPEND CMAKE_CONFIGURATION_TYPES DebugOptEmulation)
    list(REMOVE_DUPLICATES CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING "Add DebugOptEmulation" FORCE)
    get_directory_property(_vars VARIABLES)

    foreach(_var IN LISTS _vars)
        if (_var MATCHES "_DEBUG$")
            string(REPLACE "_DEBUG" "_DEBUGOPTEMULATION" _var_new "${_var}")
            set(${_var_new} "${${_var}}")
        endif()
    endforeach()

    # Remove /RTC1 from flags and add them to the executable
    STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUGOPTEMULATION "${CMAKE_C_FLAGS_DEBUGOPTEMULATION}")
    STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUGOPTEMULATION "${CMAKE_CXX_FLAGS_DEBUGOPTEMULATION}") 
endif()

configure_file(src/config.h.in "${PROJECT_BINARY_DIR}/config.h")

add_subdirectory(libs/glfw)
add_subdirectory(libs/gl3w)

add_executable(${PROJECT_NAME}
    libs/imgui/imgui.cpp
    libs/imgui/imgui_demo.cpp
    libs/imgui/imgui_draw.cpp
    libs/imgui/imgui_tables.cpp
    libs/imgui/imgui_widgets.cpp
    libs/imgui/misc/cpp/imgui_stdlib.cpp
    
    libs/imgui/backends/imgui_impl_glfw.cpp
    libs/imgui/backends/imgui_impl_opengl3.cpp
    
    libs/ImGuiFileDialog/ImGuiFileDialog.cpp
    
    src/application.cpp
    src/main_application.cpp
    
    src/systems/comment.cpp
    src/systems/expressions.cpp
    src/systems/system.cpp
    src/systems/nes/apu_io.cpp
    src/systems/nes/cartridge.cpp
    src/systems/nes/comment.cpp
    src/systems/nes/cpu.cpp
    src/systems/nes/defines.cpp
    src/systems/nes/disasm.cpp
    src/systems/nes/enum.cpp
    src/systems/nes/expressions.cpp
    src/systems/nes/label.cpp
    src/systems/nes/memory.cpp
    src/systems/nes/ppu.cpp
    src/systems/nes/system.cpp
    
    src/windows/baseproject.cpp
    src/windows/basewindow.cpp
    src/windows/main.cpp
    src/windows/projectcreator.cpp
    
    src/windows/nes/defines.cpp
    src/windows/nes/emulator.cpp
    src/windows/nes/enums.cpp
    src/windows/nes/labels.cpp
    src/windows/nes/listing.cpp
    src/windows/nes/listingitems.cpp
    src/windows/nes/project.cpp
    src/windows/nes/quickexpressions.cpp
    src/windows/nes/references.cpp
    src/windows/nes/regions.cpp
)

# from https://stackoverflow.com/questions/74426638/how-to-remove-rtc1-from-specific-target-or-file-in-cmake
# Re-enable /RTC1 for all of DebugOptEmulation, but put the flag in COMPILE_OPTIONS for each source
add_compile_options("$<$<CONFIG:DebugOptEmulation>:/RTC1>")

# Optimize certain files
list(APPEND OPT_SOURCES src/systems/nes/nes_cpu.cpp src/systems/nes/nes_ppu.cpp)
foreach(optfile IN LISTS OPT_SOURCES)
    # remove /RTC1 on *this* file
    get_property(_var SOURCE ${optfile} PROPERTY COMPILE_OPTIONS)
    STRING(REGEX REPLACE "/RTC(su|[1su])" "" _var "${_var}")
    set_property(SOURCE ${optfile} PROPERTY COMPILE_OPTIONS "${_var}")
    # Add /O2 (speed opt) and /Ob2 (enable inlining)
    set_source_files_properties(${optfile} PROPERTIES COMPILE_OPTIONS "$<$<CONFIG:DebugOptEmulation>:/O2 /Ob2>")
endforeach()

target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/imgui/")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/imgui/backends")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/imgui/misc/cpp")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/ImGuiFileDialog")

target_link_libraries(${PROJECT_NAME} glfw gl3w)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
