
cmake_minimum_required(VERSION 3.20)

find_package(OpenGL REQUIRED)

project(GuiTemplate VERSION 0.0.1)

add_subdirectory(glfw)
add_subdirectory(gl3w)

add_executable(${PROJECT_NAME}
               src/application.cpp
               src/main.cpp

               imgui/imgui.cpp
               imgui/imgui_demo.cpp
               imgui/imgui_draw.cpp
               imgui/imgui_tables.cpp
               imgui/imgui_widgets.cpp

               imgui/backends/imgui_impl_glfw.cpp
               imgui/backends/imgui_impl_opengl3.cpp
               )

target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "imgui/")
target_include_directories(${PROJECT_NAME} PRIVATE "imgui/backends")

target_link_libraries(${PROJECT_NAME} glfw gl3w)

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
