// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 

#include <chrono>
#include <functional>
#include <iostream>
#include <string>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "util.h"
#include "application.h"
#include "windows/basewindow.h"

using namespace std::literals;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
UTIL_READVARINT_VERSION util_readvarint_version = UTIL_READVARINT_VERSION_INVALID;

// Boilerplate needed to wrap the glfw callback to our class method
// thank you to https://stackoverflow.com/questions/1000663/using-a-c-class-member-function-as-a-c-callback-function
template <typename T> struct GLFW3Callback;

template <typename RetType, typename... ParamTypes>
struct GLFW3Callback<RetType(ParamTypes...)> {
    template <typename... Args>
    static RetType glfw3_callback(Args... args) {
        callback(args...);
    }
    static std::function<RetType(ParamTypes...)> callback;
};

template <typename RetType, typename... ParamTypes>
std::function<RetType(ParamTypes...)> GLFW3Callback<RetType(ParamTypes...)>::callback;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Application::Application(std::string const& _window_title, int _window_width, int _window_height)
    : _glfw_window(0)
{
    window_title = _window_title;
    window_width = _window_width;
    window_height = _window_height;
}

Application::~Application()
{
}

int Application::Run()
{
    int err;
    if((err = CreatePlatformWindow()) < 0) return err;
    if(!OnPlatformReady()) return -1;

    main_window = CreateMainWindow();

    auto previousTime = std::chrono::steady_clock::now();

    GLFWwindow* glfw_window = (GLFWwindow*)_glfw_window;
    while (!glfwWindowShouldClose(glfw_window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Determine deltaTime
        auto currentTime = std::chrono::steady_clock::now();
        double deltaTime = (currentTime - previousTime) / 1.0s;
        previousTime = currentTime;

        // Update the app
        if(!Update(deltaTime)) break;

        // Update the main window
        if(main_window) main_window->InternalUpdate(deltaTime);

        // Render the GUI layer
        ImGui_ImplOpenGL3_NewFrame();     // Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if(main_window) main_window->InternalRender();

        // Clear screen and allow the implementation to render opengl if it wants to
        int display_w, display_h;
        glfwGetFramebufferSize(glfw_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color[0] * clear_color[3], clear_color[1] * clear_color[3], clear_color[2] * clear_color[3], clear_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);
        RenderGL();

        // Push the GUI state to opengl
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional platform windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        // flip
        glfwSwapBuffers(glfw_window);
    }

    DestroyPlatformWindow();
    OnPlatformClosed();

    return 0;
}

void Application::SetTitle(std::string const& s)
{
    window_title = s;
    GLFWwindow* glfw_window = (GLFWwindow*)_glfw_window;
    glfwSetWindowTitle(glfw_window, window_title.c_str());
}

int Application::CreatePlatformWindow()
{
    // Setup window
    //glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return -1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    // Center the glfw_window on the first monitor
    int mcount;
    GLFWmonitor** monitors = glfwGetMonitors(&mcount);
    const GLFWvidmode* video_mode = glfwGetVideoMode(monitors[0]);
    int monitor_x, monitor_y;
    glfwGetMonitorPos(monitors[0], &monitor_x, &monitor_y);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Create window with graphics context
    GLFWwindow* glfw_window = glfwCreateWindow(window_width, window_height, window_title.c_str(), NULL, NULL);
    if (glfw_window == NULL) return -2;
    _glfw_window = (uintptr_t)glfw_window;

    // move the window
    glfwDefaultWindowHints(); // required for moving the window?
    SetWindowPos(monitor_x + (video_mode->width - window_width) / 2, monitor_y + (video_mode->height - window_height) / 2); 
    glfwShowWindow(glfw_window);

    //int xpos, ypos;
    //glfwGetWindowPos(glfw_window, &xpos, &ypos);
    //std::cout << "window at " << xpos << ", " << ypos << std::endl;

    // Bind the keypress handler
    // static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    typedef void (*glfw3_callback_t)(GLFWwindow*, int, int, int, int);
    GLFW3Callback<void(GLFWwindow*, int, int, int, int)>::callback = std::bind(&Application::KeyPressHandler, this, 
                                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
    glfw3_callback_t key_press_handler = static_cast<glfw3_callback_t>(GLFW3Callback<void(GLFWwindow*, int, int, int, int)>::glfw3_callback);
    glfwSetKeyCallback(glfw_window, key_press_handler);

    // static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    typedef void (*glfw3_window_pos_callback_t)(GLFWwindow*, int, int);
    GLFW3Callback<void(GLFWwindow*, int, int)>::callback = std::bind(&Application::WindowPosHandler, this, 
                                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
    glfw3_window_pos_callback_t window_pos_handler = static_cast<glfw3_window_pos_callback_t>(GLFW3Callback<void(GLFWwindow*, int, int)>::glfw3_callback);
    glfwSetWindowPosCallback(glfw_window, window_pos_handler);

    // Make context current & enable vsync
    glfwMakeContextCurrent(glfw_window);
    glfwSwapInterval(1);

    // Try initializing opengl
    if (gl3wInit() != 0) {
        std::cerr << "failed to initialize gl3w!\n" << std::endl;
        return -3;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(glfw_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != NULL);

    return 0;
}
 
void Application::DestroyPlatformWindow()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow((GLFWwindow*)_glfw_window);
    glfwTerminate();
}

void Application::KeyPressHandler(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    if(_window != (GLFWwindow*)_glfw_window) return;

    // only dispatch the event if ImGui says it's OK
    ImGuiIO& io = ImGui::GetIO();
    if(!io.WantCaptureKeyboard) {
        OnKeyPress(key, scancode, action, mods);
    }
}

void Application::WindowPosHandler(GLFWwindow* _window, int x, int y)
{
    OnWindowMoved(x, y);
}

void Application::SetWindowPos(int x, int y)
{
    glfwSetWindowPos((GLFWwindow*)_glfw_window, x, y);
}

bool Application::Update(double deltaTime) { return true; }

void Application::RenderGL() { }

void Application::OnKeyPress(int glfw_key, int scancode, int action, int mods) { }

void Application::OnWindowMoved(int, int) { }

bool Application::OnPlatformReady() { return true; }

void Application::OnPlatformClosed() { }

