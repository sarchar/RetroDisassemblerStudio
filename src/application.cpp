
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

#include "application.h"

using namespace std::literals;

static char const * const DOCKSPACE_NAME = "RootDockspace";

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
    : enable_toolbar(false), enable_statusbar(false), has_dock_builder(false)
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
    if((err = CreateWindow()) < 0) return err;

    if(!OnWindowCreated()) return -1;

    auto previousTime = std::chrono::steady_clock::now();

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

        // Render the GUI layer
        ImGui_ImplOpenGL3_NewFrame();     // Start the Dear ImGui frame
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        RenderGUI();
        _RenderMainStatusBar();

        // Clear screen and allow the implementation to render opengl if it wants to
        int display_w, display_h;
        glfwGetFramebufferSize(glfw_window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
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

    DestroyWindow();
    OnWindowDestroyed();

    return 0;
}

int Application::CreateWindow()
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
    glfw_window = glfwCreateWindow(window_width, window_height, window_title.c_str(), NULL, NULL);
    if (glfw_window == NULL) return -2;

    // move the window
    glfwDefaultWindowHints(); // required for moving the window?
    SetWindowPos(monitor_x + (video_mode->width - window_width) / 2, monitor_y + (video_mode->height - window_height) / 2); 
    glfwShowWindow(glfw_window);

    int xpos, ypos;
    glfwGetWindowPos(glfw_window, &xpos, &ypos);
    std::cout << "window at " << xpos << ", " << ypos << std::endl;

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
 
void Application::DestroyWindow()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(glfw_window);
    glfwTerminate();
}

void Application::ShowDockSpace(bool dockSpaceHasBackground)
{
    // code from https://gist.github.com/PossiblyAShrub/0aea9511b84c34e191eaa90dd7225969
    static ImGuiDockNodeFlags dockspace_flags = 0;
    if (!dockSpaceHasBackground) dockspace_flags |= ImGuiDockNodeFlags_PassthruCentralNode;
    
    // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
    // because it would be confusing to have two docking targets within each others.
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

    // Adjust for the status bar, if shown
    float status_bar_height = 0;
    if(enable_statusbar) {
        status_bar_height = ImGui::GetFrameHeight();
    }
     
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImVec2 dockspace_size = viewport->Size;
    dockspace_size.y -= status_bar_height;
    ImGui::SetNextWindowSize(dockspace_size);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
   
    // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background and 
    // handle the pass-thru hole, so we ask Begin() to not render a background.
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) window_flags |= ImGuiWindowFlags_NoBackground;
    
    // Important: note that we proceed even if Begin() returns false (aka window is collapsed).
    // This is because we want to keep our DockSpace() active. If a DockSpace() is inactive, 
    // all active windows docked into it will lose their parent and become undocked.
    // We cannot preserve the docking relationship between an active window and an inactive docking, otherwise 
    // any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleVar(2);
    
    // DockSpace
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {

        imgui_dockspace_id = ImGui::GetID(DOCKSPACE_NAME);
        ImGui::DockSpace(imgui_dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        if (!has_dock_builder)
        {
            ImGui::DockBuilderRemoveNode(imgui_dockspace_id); // clear any previous layout

            // Create the root node, which we can use to dock windows
            imgui_dock_builder_root_id = ImGui::DockBuilderAddNode(imgui_dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);

            // And make it take the entire viewport
            ImGui::DockBuilderSetNodeSize(imgui_dockspace_id, viewport->Size);

            // split the dockspace into left and right, with the right side a temporary ID
            ImGuiID right_id;
            imgui_dock_builder_left_id = ImGui::DockBuilderSplitNode(imgui_dock_builder_root_id, ImGuiDir_Left, 0.3f, nullptr, &right_id);

            // split the right area, creating a temporary middle
            ImGuiID middle_id;
            imgui_dock_builder_right_id = ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Right, 0.5f, nullptr, &middle_id);

            // now split the middle area into a top and bottom
            imgui_dock_builder_bottom_id = ImGui::DockBuilderSplitNode(middle_id, ImGuiDir_Down, 0.5f, nullptr, nullptr);

            ImGui::DockBuilderFinish(imgui_dockspace_id);

            // do this last for race conditions
            has_dock_builder = true;
        }
    }

    RenderMainMenuBar();
    _RenderMainToolBar();
    ImGui::End();
}

// Render a tool/status bars in the application window
// see https://github.com/ocornut/imgui/issues/3518#issuecomment-807398290
void Application::_RenderMainToolBar()
{
    if(!enable_toolbar) return;

    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    float height = ImGui::GetFrameHeight();

    if (ImGui::BeginViewportSideBar("##SecondaryMenuBar", viewport, ImGuiDir_Up, height, window_flags)) {
        if (ImGui::BeginMenuBar()) {
            RenderMainToolBar();
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

void Application::_RenderMainStatusBar()
{
    if(!enable_statusbar) return;

    ImGuiViewportP* viewport = (ImGuiViewportP*)(void*)ImGui::GetMainViewport();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    float height = ImGui::GetFrameHeight();

    if (ImGui::BeginViewportSideBar("##MainStatusBar", viewport, ImGuiDir_Down, height, window_flags)) {
        if(ImGui::BeginMenuBar()) {
            RenderMainStatusBar();
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

void Application::KeyPressHandler(GLFWwindow* _window, int key, int scancode, int action, int mods)
{
    if(_window != glfw_window) return;

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
    glfwSetWindowPos(glfw_window, x, y);
}

bool Application::Update(double deltaTime) { return true; }

void Application::RenderGL() { }

void Application::RenderGUI() { }

void Application::RenderMainMenuBar() { }

void Application::RenderMainStatusBar() { }

void Application::RenderMainToolBar() { } 

void Application::OnKeyPress(int glfw_key, int scancode, int action, int mods) { }

void Application::OnWindowMoved(int, int) { }

void Application::OnWindowDestroyed() { }

bool Application::OnWindowCreated() { return true; }


