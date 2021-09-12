
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
    : enable_toolbar(false), enable_statusbar(false)
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

    // Create window with graphics context
    glfw_window = glfwCreateWindow(window_width, window_width, window_title.c_str(), NULL, NULL);
    if (glfw_window == NULL) return -2;

    // Bind the keypress handler
    // static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    typedef void (*glfw3_callback_t)(GLFWwindow*, int, int, int, int);
    GLFW3Callback<void(GLFWwindow*, int, int, int, int)>::callback = std::bind(&Application::KeyPressHandler, this, 
                                    std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5);
    glfw3_callback_t key_press_handler = static_cast<glfw3_callback_t>(GLFW3Callback<void(GLFWwindow*, int, int, int, int)>::glfw3_callback);
    glfwSetKeyCallback(glfw_window, key_press_handler);

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
    
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
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

        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

        static auto first_time = true;
        if (first_time)
        {
            first_time = false;

            ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
            ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            //! // split the dockspace into 2 nodes -- DockBuilderSplitNode takes in the following args in the following order
            //! //   window ID to split, direction, fraction (between 0 and 1), the final two setting let's us choose which id we want (which ever one we DON'T set as NULL, will be returned by the function)
            //! //                                                              out_id_at_dir is the id of the node in the direction we specified earlier, out_id_at_opposite_dir is in the opposite direction
            //! auto dock_id_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.2f, nullptr, &dockspace_id);
            //! auto dock_id_down = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.25f, nullptr, &dockspace_id);

            //! // we now dock our windows into the docking node we made above
            //! ImGui::DockBuilderDockWindow("Down", dock_id_down);
            //! ImGui::DockBuilderDockWindow("Left", dock_id_left);
            //! ImGui::DockBuilderFinish(dockspace_id);
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

void Application::OnWindowDestroyed() { }

bool Application::OnWindowCreated() { return true; }

