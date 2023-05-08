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
#include "systems/nes/nes_system.h"
#include "systems/snes/snes_system.h"
#include "windows/debugger.h"
#include "windows/memory.h"
#include "windows/rom_loader.h"
#include "windows/nes/listing.h"

#undef DISABLE_IMGUI_SAVE_LOAD_LAYOUT

using namespace std;

MyApp::MyApp(int argc, char* argv[])
    : Application("Retro Disassembler Studio", 1600, 1000),
      request_exit(false), show_imgui_demo(false)
{
    ((void)argc);
    ((void)argv);
    ProjectCreatorWindow::RegisterSystemInformation(NESSystem::GetInformationStatic());
    ProjectCreatorWindow::RegisterSystemInformation(SNESSystem::GetInformationStatic());
    current_system_changed = make_shared<current_system_changed_t>();

    // register all the windows
#   define REGISTER_WINDOW_TYPE(className) \
        create_window_functions[className::GetWindowClassStatic()] = std::bind(&className::CreateWindow);
    REGISTER_WINDOW_TYPE(NES::Listing);
    REGISTER_WINDOW_TYPE(SNESMemory);
    REGISTER_WINDOW_TYPE(SNESDebugger);
#   undef REGISTER_WINDOW_TYPE
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
    get_user_config_folder(cfgdir, sizeof(cfgdir), PROJECT_NAME);
    string config_dir(cfgdir);
    layout_file = config_dir + PATH_SEPARATOR_STRING + "imgui_layout.ini";
    cout << layout_file << endl;
    io.IniFilename = layout_file.c_str();
#endif

    cout << "[MyApp] ImGui layout file is " << io.IniFilename << endl;

    // Connect handlers for ImGui to store layout data
    SetupINIHandlers();

    // position the window in a consistent location
#ifndef NDEBUG
    SetWindowPos(1600, 200);
#endif

    // load some fonts
    // TODO everything will one day be user customizable
    ImFont* default_font = io.Fonts->AddFontDefault();

    main_font = io.Fonts->AddFontFromFileTTF("ext/iosevka-regular.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    if(main_font == nullptr) {
        cout << "[MyApp] Warning: unable to load iosevka-regular.ttf. Using default font." << endl;
        main_font = default_font;
    }

    main_font_bold = io.Fonts->AddFontFromFileTTF("ext/iosevka-heavy.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesDefault());
    if(main_font_bold == nullptr) { 
        cout << "[MyApp] Warning: unable to load iosevka-bold.ttf. Using default font." << endl;
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

    // Create a style
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding.x    = 0;
    style.WindowPadding.y    = 0;
    style.FramePadding.x     = 3;
    style.FramePadding.y     = 3;
    style.CellPadding.x      = 2;
    style.CellPadding.y      = 1;
    style.ItemSpacing.x      = 8;
    style.ItemSpacing.y      = 5;
    style.ScrollbarSize      = 12;
    style.GrabMinSize        = 13;
    style.WindowTitleAlign.x = 0.5f;

    return true;
}

void MyApp::SetupINIHandlers()
{
    ImGuiSettingsHandler ini_handler;

    ini_handler.TypeName = "RetroGameDisassemblerLayout";
    ini_handler.TypeHash = ImHashStr("RetroGameDisassemblerLayout");
    
    ini_handler.ClearAllFn = [](ImGuiContext*, ImGuiSettingsHandler*) {
        // TODO not sure when this is called
        assert(false); 
    };

    ini_handler.ReadOpenFn = [](ImGuiContext*, ImGuiSettingsHandler*, char const* name) -> void* {
        // name contains the value in the second set of []. we don't use it, we just assume
        // the order is correct, and if it isn't, it really isn't a big deal
        WindowFromINI* wfini = MyApp::Instance()->NewINIWindow();
        return (void*)wfini;
    };

    ini_handler.ReadLineFn = [](ImGuiContext*, ImGuiSettingsHandler*, void* entry, const char* line) {
        // for each line within the ini section, this function gets called
        WindowFromINI* wfini = (WindowFromINI*)entry;

        char buffer[64];
        if(sscanf(line, "WindowClass=%63[^\r\n]", buffer) == 1) {
            wfini->window_class = string(buffer);
        } else if(sscanf(line, "WindowID=%16[^\r\n]", buffer) == 1) {
            wfini->window_id = string(buffer);
        }

    };

    ini_handler.ApplyAllFn = [](ImGuiContext*, ImGuiSettingsHandler*) {
        // after the entire ini file is loaded, this function is called and we create the windows
        MyApp::Instance()->CreateINIWindows();
    };

    ini_handler.WriteAllFn = [](ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
        // this function is called to output data to the ini file

        // loop over all managed windows and add them to the ini file under their own heading
        MyApp* instance = MyApp::Instance();
        int window_index = 0;
        for(auto &window : instance->managed_windows) {
            buf->appendf("[%s][%d]\n", handler->TypeName, window_index);
            buf->appendf("WindowClass=%s\n", window->GetWindowClass());
            buf->appendf("WindowID=%s\n", window->GetWindowID().c_str());
            buf->appendf("\n");
            window_index++;
        }
    };

    // add the handler to the ImGuiContext
    ImGuiContext& g = *ImGui::GetCurrentContext();
    g.SettingsHandlers.push_back(ini_handler);
}

MyApp::WindowFromINI* MyApp::NewINIWindow()
{
    shared_ptr<WindowFromINI> wfini = make_shared<WindowFromINI>();
    ini_windows.push_back(wfini);
    return wfini.get(); // considered unsafe, but I know it's not stored for use later
}

void MyApp::CreateINIWindows()
{
#if 0 // TODO temporarily disabling the creationg of the windows from the INI file.
      // later, we will want to recreate the last open project. or not? let the user pick.
    // loop over all the INI windows and create them
    for(auto& wfini : ini_windows) {
        if(!create_window_functions.contains(wfini->window_class)) {
            cout << "[MyApp] warning: class type " << wfini->window_class << " from INI doesn't exist" << endl;
            continue;
        }

        // create the window
        auto& create_function = create_window_functions[wfini->window_class];
        auto wnd = create_function();

        // set the ID to match the one in the file
        wnd->SetWindowID(wfini->window_id);

        // add it to the managed windows list
        AddWindow(wnd);
    }
#endif

    // free memory
    ini_windows.clear();
}

void MyApp::AddWindow(shared_ptr<BaseWindow> window)
{
    // bind to window_closed which is available on all BaseWindows
    *window->window_closed += std::bind(&MyApp::ManagedWindowClosedHandler, this, placeholders::_1);

    managed_windows.push_back(window);
    cout << "[MyApp] Added window \"" << window->GetTitle() << "\" (managed window count = " << managed_windows.size() << ")" << endl;
}

void MyApp::ManagedWindowClosedHandler(std::shared_ptr<BaseWindow> window)
{
    cout << "[MyApp] \"" << window->GetTitle() << "\" closed (managed window count = " << managed_windows.size() + queued_windows_for_delete.size() - 1 << ")" << endl;
    queued_windows_for_delete.push_back(window);
}

void MyApp::ProcessQueuedWindowsForDelete()
{
    for(auto& window : queued_windows_for_delete) {
        auto it = find(managed_windows.begin(), managed_windows.end(), window);
        if(it != managed_windows.end()) managed_windows.erase(it);
    }

    queued_windows_for_delete.resize(0);
}

bool MyApp::Update(double deltaTime)
{
    // Update all open windows
    for(auto &window : managed_windows) {
        window->Update(deltaTime);
    }

    // Remove any windows queued for deletion
    ProcessQueuedWindowsForDelete();

    return !request_exit;
}

void MyApp::OpenROMInfosPane()
{
    static string last_file_selection;
    static struct {
        u32 prg_rom;
        u32 chr_rom;

        u8 prg_rom_banks;
        u8 chr_rom_banks;

        u8 mapper;
        bool vertical_mirroring;
        bool four_screen;
        bool has_sram;
        bool has_trainer;

        bool valid;
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

            unsigned char buf[16];
            rom_stream.read(reinterpret_cast<char*>(buf), 16);
            if(rom_stream && (buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
                rom_info.prg_rom_banks      = (u8)buf[4];
                rom_info.prg_rom            = rom_info.prg_rom_banks * 16 * 1024;
                rom_info.chr_rom_banks      = (u8)buf[5];
                rom_info.chr_rom            = rom_info.chr_rom_banks * 8 * 1024;

                rom_info.mapper             = ((u8)buf[6] & 0xF0) >> 4 | ((u8)buf[7] & 0xF0);

                rom_info.vertical_mirroring = (bool)((u8)buf[6] & 0x01);
                rom_info.has_sram           = (bool)((u8)buf[6] & 0x02);
                rom_info.has_trainer        = (bool)((u8)buf[6] & 0x04);
                rom_info.four_screen        = (bool)((u8)buf[6] & 0x08);

                rom_info.valid = true;
            } else {
                rom_info.valid = false;
            }

            last_file_selection = file_path_name;
        }


        if(rom_info.valid) {
            stringstream ss;

            ////////////////////////////////////////////////////////////
            ImGui::Text("Mapper: ");
            ImGui::SameLine();
            ImGui::Text("%d", rom_info.mapper);

            ////////////////////////////////////////////////////////////
            ss.imbue(locale(""));
            ss << fixed;

            if(rom_info.prg_rom >= (1024*1024)) {
                ss << (rom_info.prg_rom / (1024 * 1024));
                ss << " MiB";
            } else if(rom_info.prg_rom >= 1024) {
                ss << (rom_info.prg_rom / 1024);
                ss << " KiB";
            } else {
                ss << rom_info.prg_rom;
                ss << " B";
            }

            ss << " (" << (int)rom_info.prg_rom_banks << " banks)";

            ImGui::Text("PRG: ");
            ImGui::SameLine();
            ImGui::Text("%s", ss.str().c_str());

            ////////////////////////////////////////////////////////////
            ss.str("");
            ss.clear();
            ss.imbue(locale(""));
            ss << fixed;

            if(rom_info.chr_rom >= (1024*1024)) {
                ss << (rom_info.chr_rom / (1024 * 1024));
                ss << " MiB";
            } else if(rom_info.chr_rom >= 1024) {
                ss << (rom_info.chr_rom / 1024);
                ss << " KiB";
            } else {
                ss << rom_info.chr_rom;
                ss << " B";
            }

            ss << " (" << (int)rom_info.chr_rom_banks << " banks)";

            ImGui::Text("CHR: ");
            ImGui::SameLine();
            ImGui::Text("%s", ss.str().c_str());

            ////////////////////////////////////////////////////////////
            ImGui::Text("Mirroring: ");
            ImGui::SameLine();
            ImGui::Text(rom_info.four_screen ? "None" : (rom_info.vertical_mirroring ? "Vertical" : "Horizontal"));

            ////////////////////////////////////////////////////////////
            ImGui::Text("SRAM: ");
            ImGui::SameLine();
            ImGui::Text(rom_info.has_sram ? "Present" : "Not Present");

            ////////////////////////////////////////////////////////////
            ImGui::Text("Trainer: ");
            ImGui::SameLine();
            ImGui::Text(rom_info.has_trainer ? "Present" : "Not Present");
        } else {
            ImGui::Text("Not a valid ROM");
        }
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

                ImGuiFileDialog::Instance()->OpenDialog("OpenROMFileDialog", "Choose ROM to disassemble", "NES ROMs (*.nes){.nes}", "./roms/", "",
                                                       bind(infos_pane_cb, placeholders::_1, placeholders::_2, placeholders::_3),
                                                       250, 1, IGFDUserDatas("InfosPane"), ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtention
                                                                                           | ImGuiFileDialogFlags_DisableCreateDirectoryButton);
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
                if(dir != nullptr) {
                    dirent* dirent;
                    while((dirent = readdir(dir)) != nullptr) {
                        string file_path_name = string("roms/") + string(dirent->d_name);
                        if(ends_with(file_path_name, ".nes")) {
                            test_roms.push_back(file_path_name);
                        }
                    }
                    closedir(dir);
                }
            }

            for(auto &t : test_roms) {
                if(ImGui::MenuItem(t.c_str())) {
                    CreateNewProject(t);
                }
            }

            ImGui::EndMenu();
        } else {
            if(test_roms.size() > 0) {
                test_roms.resize(0);
            }
        }

        if(ImGui::BeginMenu("Windows")) {
            if(ImGui::MenuItem("Debugger")) {
                auto wnd = SNESDebugger::CreateWindow();
                AddWindow(wnd);
            }
            if(ImGui::MenuItem("Memory")) {
                auto wnd = SNESMemory::CreateWindow();
                AddWindow(wnd);
            }
            ImGui::EndMenu();
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
                    CreateNewProject(file_path_name);
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
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    // Only call ShowDockSpace if you want your main window to be a dockable workspace
    ShowDockSpace();

    // Show the ImGui demo window if requested
    if(show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

    // Render all open windows
    for(auto &window : managed_windows) {
        window->RenderGUI();
    }

    // Remove any windows queued for deletion
    ProcessQueuedWindowsForDelete();

    // CLean up style vars
    ImGui::PopStyleVar(1);
}

bool MyApp::OKPopup(std::string const& title, std::string const& message)
{
    ImGui::OpenPopup(title.c_str());

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool ret = false;
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize)) {
        // TODO get word wrapping working. For now it just seems broken.
        //ImVec2 size = ImGui::GetMainViewport()->Size;
        //ImGui::PushTextWrapPos(size.x * 0.67);//ImGui::GetContentRegionAvailWidth());
        //ImGui::TextUnformatted(message.c_str());
        ImGui::Text(message.c_str());

        if (ImGui::Button("OK", ImVec2(120, 0))) { 
            ImGui::CloseCurrentPopup(); 
            ret = true;
        }

        //ImGui::PopTextWrapPos();
        ImGui::EndPopup();
    }

    return ret;
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

void MyApp::CreateNewProject(string const& file_path_name)
{
    cout << "[MyApp] CreateNewProject(" << file_path_name << ")" << endl;

    // TODO: close the current project and open windows
    for(auto& window : managed_windows) {
        window->CloseWindow();
    }

    auto loader = ProjectCreatorWindow::CreateWindow(file_path_name);
    *loader->system_loaded += std::bind(&MyApp::SystemLoadedHandler, this, placeholders::_1, placeholders::_2);
    AddWindow(loader);
}

void MyApp::SystemLoadedHandler(std::shared_ptr<BaseWindow> project_creator_window, std::shared_ptr<System> new_system)
{
    project_creator_window->CloseWindow();

    // TODO: create the default workspace and focus a source editor to the projects entry point

    current_system = new_system;
    cout << "[MyApp] new " << current_system->GetInformation()->full_name << " loaded." << endl;
    current_system_changed->emit();

    // TODO Would be nice to look up the Listing window and not care which platform is loaded
    auto wnd = NES::Listing::CreateWindow();
    AddWindow(wnd);
}

int main(int argc, char* argv[])
{
    return MyApp::Instance(argc, argv)->Run();
}
