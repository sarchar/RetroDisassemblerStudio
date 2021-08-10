#include "config.h"

#include <iostream>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "cfgpath.h"

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"

MyApp::MyApp(int argc, char* argv[])
    : Application("ImGUI Docking Template", 1000, 800),
      request_exit(false), show_imgui_demo(false)
{
    ((void)argc);
    ((void)argv);
}

MyApp::~MyApp()
{
}

bool MyApp::OnWindowCreated()
{
    clear_color = ImVec4(0.9375, 0.9453125, 0.95703125, 1.0);

    // show a status bar
    SetEnableStatusBar(true);

    // set filename to null to disable auto-save/load of ImGui layouts
    // and use ImGui::SaveIniSettingsToDisk() instead.
    ImGuiIO& io = ImGui::GetIO();
#if defined(DISABLE_IMGUI_SAVE_LOAD_LAYOUT)
    io.IniFilename = nullptr;
#else
    char cfgdir[MAX_PATH];
    get_user_config_folder(cfgdir, sizeof(cfgdir), "myapp");
    std::string config_dir(cfgdir);
    layout_file = config_dir + PATH_SEPARATOR_STRING + "myapp.ini";
    io.IniFilename = layout_file.c_str();
#endif

    // position the window in a consistent location
    SetWindowPos(800, 200);

    return true;
}

bool MyApp::Update(double deltaTime)
{
    return !request_exit;
}

void MyApp::RenderMainMenuBar()
{
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Show ImGui Demo", "ctrl+d")) {
                show_imgui_demo = true;
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "ctrl+x")) {
                request_exit = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void MyApp::RenderMainStatusBar() 
{
    ImGui::Text("Happy status bar");
}

void MyApp::RenderGUI()
{
    // Only call ShowDockSpace if you want your main window to be a dockable workspace
    ShowDockSpace();

    // Show the ImGui demo window if requested
    if (show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

    // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
    {
        static float f = 0.0f;
        static int counter = 0;
    
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
    
        ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
        ImGui::Checkbox("Demo Window", &show_imgui_demo);       // Edit bools storing our window open/close state
    
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
        ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color
    
        if (ImGui::Button("Button")) counter++;                 // Buttons return true when clicked (most widgets return true when edited/activated)

        ImGui::SameLine();
        ImGui::Text("counter = %d", counter);
    
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
    }

}

void MyApp::OnKeyPress(int glfw_key, int scancode, int action, int mods)
{
    if(action != GLFW_PRESS) return;

    if((mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == GLFW_MOD_CONTROL) {
        switch(glfw_key) {
        case GLFW_KEY_D:
            show_imgui_demo = true;
            break;
        case GLFW_KEY_X:
            request_exit = true;
            break;
        }
    } else if((mods & (GLFW_MOD_CONTROL | GLFW_MOD_ALT)) == GLFW_MOD_ALT) {
        switch(glfw_key) {
        case GLFW_KEY_F: 
            // TODO open the File menu
            // As it stands right now there doesn't seem to be a way to do something like this in ImGui:
            // ActivateMenu("File")
            break;
        }
    }
}

int main(int argc, char* argv[])
{
    MyApp app(argc, argv);
    return app.Run();
}
