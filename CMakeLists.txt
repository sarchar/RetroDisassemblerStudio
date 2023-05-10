
cmake_minimum_required(VERSION 3.20)

find_package(OpenGL REQUIRED)

project(RetroDisassemblerStudio VERSION 0.0.2)

configure_file(src/config.h.in "${PROJECT_BINARY_DIR}/config.h")

add_subdirectory(libs/glfw)
add_subdirectory(libs/gl3w)

add_executable(${PROJECT_NAME}
               libs/imgui/imgui.cpp
               libs/imgui/imgui_demo.cpp
               libs/imgui/imgui_draw.cpp
               libs/imgui/imgui_tables.cpp
               libs/imgui/imgui_widgets.cpp

               libs/imgui/backends/imgui_impl_glfw.cpp
               libs/imgui/backends/imgui_impl_opengl3.cpp

               libs/ImGuiFileDialog/ImGuiFileDialog.cpp

               src/application.cpp
               src/systems/expressions.cpp
               src/main.cpp
               src/system_clock.cpp

               src/systems/system.cpp
               src/systems/nes/nes_cartridge.cpp
               src/systems/nes/nes_disasm.cpp
               src/systems/nes/nes_expressions.cpp
               src/systems/nes/nes_label.cpp
               src/systems/nes/nes_listing.cpp
               src/systems/nes/nes_memory.cpp
               src/systems/nes/nes_system.cpp
               src/systems/snes/cpu65c816.cpp
               src/systems/snes/snes_address_decoder.cpp
               src/systems/snes/snes_system.cpp

               src/windows/base_window.cpp
               src/windows/debugger.cpp
               src/windows/memory.cpp
               src/windows/rom_loader.cpp

               src/windows/nes/listing.cpp
               )

target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/imgui/")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/imgui/backends")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/ImGuiFileDialog")

target_link_libraries(${PROJECT_NAME} glfw gl3w)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
