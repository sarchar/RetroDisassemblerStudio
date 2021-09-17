
cmake_minimum_required(VERSION 3.20)

find_package(OpenGL REQUIRED)

project(RetroDisassemblerStudio VERSION 0.0.1)

configure_file(src/config.h.in "${PROJECT_BINARY_DIR}/config.h")

add_subdirectory(glfw)
add_subdirectory(gl3w)

add_executable(${PROJECT_NAME}
               src/application.cpp
               src/main.cpp
               src/system_clock.cpp

               src/systems/system.cpp
               src/systems/snes/cpu65c816.cpp
               src/systems/snes/snes_address_decoder.cpp
               src/systems/snes/snes_system.cpp

               src/windows/base_window.cpp
               src/windows/debugger.cpp
               src/windows/memory.cpp
               src/windows/rom_loader.cpp

               imgui/imgui.cpp
               imgui/imgui_demo.cpp
               imgui/imgui_draw.cpp
               imgui/imgui_tables.cpp
               imgui/imgui_widgets.cpp

               imgui/backends/imgui_impl_glfw.cpp
               imgui/backends/imgui_impl_opengl3.cpp

               libs/ImGuiFileDialog/ImGuiFileDialog.cpp
               )

target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}")
target_include_directories(${PROJECT_NAME} PRIVATE "imgui/")
target_include_directories(${PROJECT_NAME} PRIVATE "imgui/backends")
target_include_directories(${PROJECT_NAME} PRIVATE "libs/ImGuiFileDialog")

target_link_libraries(${PROJECT_NAME} glfw gl3w)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
