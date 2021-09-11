#include "config.h"

#include <iostream>
#include <locale>
#include <sstream>
#include <vector>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "cfgpath.h"

#include "imgui.h"
#include "imgui_internal.h"

#define USE_IMGUI_TABLES
#include "ImGuiFileDialog.h"
#include "dirent/dirent.h"

#include "main.h"
#include "rom_loader.h"

using namespace std;

MyApp::MyApp(int argc, char* argv[])
    : Application("Retro Disassembler Studio", 1000, 800),
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
    string config_dir(cfgdir);
    layout_file = config_dir + PATH_SEPARATOR_STRING + "myapp.ini";
    io.IniFilename = layout_file.c_str();
#endif

    // position the window in a consistent location
#ifndef NDEBUG
    SetWindowPos(1600, 200);
#endif

    // load some fonts
    ImFont* default_font = io.Fonts->AddFontDefault();

    main_font = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\iosevka-regular.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    if(main_font == nullptr) {
        cout << "Warning: unable to load iosevka-regular.ttf. Using default font." << endl;
        main_font = default_font;
    }

    main_font_bold = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\iosevka-heavy.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    if(main_font_bold == nullptr) { 
        cout << "Warning: unable to load iosevka-bold.ttf. Using default font." << endl;
        main_font_bold = default_font;
    }

    IM_ASSERT(main_font != NULL);
    IM_ASSERT(main_font_bold != NULL);

    // replace the default font
    if(main_font != default_font) {
        io.FontDefault = static_cast<ImFont*>(main_font);
    }

    // scale up some
    io.FontGlobalScale = 1.2f;

    return true;
}

void MyApp::AddWindow(BaseWindow* window)
{
    unique_ptr<BaseWindow> ptr(window);
    managed_windows.push_back(std::move(ptr));
    cout << "managed window count = " << managed_windows.size() << endl;
}

bool MyApp::Update(double deltaTime)
{
    // Update all open windows
    for(auto &window : managed_windows) {
        window->Update(deltaTime);
    }

    return !request_exit;
}

void MyApp::OpenROMInfosPane()
{
    static string last_file_selection;
    static struct {
        unsigned long rom_size;
        unsigned long load_address;
    } rom_info = { 0, };

    ImGui::PushFont(static_cast<ImFont*>(main_font_bold));
    ImGui::Text("ROM info");
    ImGui::PopFont();

    auto selection = ImGuiFileDialog::Instance()->GetSelection();
    if(selection.size() > 0) {
        // cache the ROM info as long as the filename hasn't changed
        string file_path_name = (*selection.begin()).second;
        if(file_path_name != last_file_selection) {
            ifstream rom_stream(file_path_name, ios::binary);

            unsigned char buf[2];
            rom_stream.read(reinterpret_cast<char*>(buf), 2);
            rom_info.load_address = buf[0] | (buf[1] << 8);

            rom_stream.read(reinterpret_cast<char*>(buf), 2);
            rom_info.rom_size = buf[0] | (buf[1] << 8);

            last_file_selection = file_path_name;
        }


        stringstream ss;
        ss.imbue(locale(""));
        ss << fixed;

        string mult;
        if(rom_info.rom_size >= (1024*1024)) {
            ss << (rom_info.rom_size / (1024 * 1024));
            mult = " MiB";
        } else if(rom_info.rom_size >= 1024) {
            ss << (rom_info.rom_size / 1024);
            mult = " KiB";
        } else {
            ss << rom_info.rom_size;
            mult = " B";
        }

        ImGui::Text("Size: ");
        ImGui::SameLine();
        ImGui::Text(ss.str().c_str());
        ImGui::SameLine();
        ImGui::Text(mult.c_str());

        ss.str(string());
        ss.imbue(locale("C")); // don't print commas in hex strings
        ss << "$" << uppercase << hex << rom_info.load_address;
        ImGui::Text("Load: ");
        ImGui::SameLine();
        ImGui::Text(ss.str().c_str());
    }
}

void MyApp::RenderMainMenuBar()
{
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("New Disassembly...", "ctrl+o")) {
                auto infos_pane_cb = [=, this](char const* vFilter, IGFDUserDatas vUserDatas, bool* cantContinue) {
                    this->OpenROMInfosPane();
                };

                ImGuiFileDialog::Instance()->OpenModal("OpenROMFileDialog", "Choose ROM to disassemble", ".bin,.smc,.nes", ".", "",
                                                       bind(infos_pane_cb, placeholders::_1, placeholders::_2, placeholders::_3),
                                                       250, 1, IGFDUserDatas("InfosPane"));
            }

            ImGui::Separator();
            if(ImGui::MenuItem("Exit", "ctrl+x")) {
                request_exit = true;
            }
            ImGui::EndMenu();
        }

        static vector<string> test_roms;
        if(ImGui::BeginMenu("Test ROMs")) {
            if(test_roms.size() == 0) { // scan for test roms
                auto ends_with = [](std::string const& value, std::string const& ending) {
                    if (ending.size() > value.size()) return false;
                    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
                };

                DIR* dir = opendir("roms");
                dirent* dirent;
                while((dirent = readdir(dir)) != nullptr) {
                    string file_path_name = string("roms/") + string(dirent->d_name);
                    if(ends_with(file_path_name, ".bin") || ends_with(file_path_name, ".smc")) {
                        test_roms.push_back(file_path_name);
                    }
                }
                closedir(dir);
            }

            for(auto &t : test_roms) {
                if(ImGui::MenuItem(t.c_str())) {
                    CreateROMLoader(t);
                }
            }

            ImGui::EndMenu();
        } else {
            if(test_roms.size() > 0) {
                test_roms.resize(0);
            }
        }

        if(ImGui::BeginMenu("Debug")) {
            if(ImGui::MenuItem("Show ImGui Demo", "ctrl+d")) {
                show_imgui_demo = true;
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        if(ImGuiFileDialog::Instance()->Display("OpenROMFileDialog")) {
            if(ImGuiFileDialog::Instance()->IsOk()) {
                auto selection = ImGuiFileDialog::Instance()->GetSelection();
                if(selection.size() >  0) {
                    string file_path_name = (*selection.begin()).second;
                    CreateROMLoader(file_path_name);
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }
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

    // Render all open windows
    for(auto &window : managed_windows) {
        window->RenderGUI();
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

void MyApp::CreateROMLoader(string const& file_path_name)
{
    cout << "CreateROMLoader(" << file_path_name << ")" << endl;
    AddWindow(ROMLoader::CreateWindow(this, file_path_name));
}

int main(int argc, char* argv[])
{
    MyApp app(argc, argv);
    return app.Run();
}
