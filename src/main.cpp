#include "config.h"

#include <filesystem>
#include <iostream>
#include <locale>
#include <sstream>
#include <vector>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#include "cfgpath.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#define USE_IMGUI_TABLES
#include "ImGuiFileDialog.h"
#include "dirent/dirent.h"

#include "main.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"
#include "windows/rom_loader.h"
#include "windows/nes/defines.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
#include "windows/nes/regions.h"

#include "systems/expressions.h" // TODO temp

#undef DISABLE_IMGUI_SAVE_LOAD_LAYOUT

using namespace std;

MyApp::MyApp(int, char*[])
    : Application("Retro Disassembler Studio", 1600, 1000),
      request_exit(false), show_imgui_demo(false)
{
    BaseProject::RegisterProjectInformation(NES::Project::GetInformationStatic());

    BaseExpressionNodeCreator::RegisterBaseExpressionNodes();
    NES::ExpressionNodeCreator::RegisterExpressionNodes();

    //
//!    current_system_changed = make_shared<current_system_changed_t>();

    // register all the windows
#   define REGISTER_WINDOW_TYPE(className) \
        create_window_functions[className::GetWindowClassStatic()] = std::bind(&className::CreateWindow);
    REGISTER_WINDOW_TYPE(NES::Listing);
    REGISTER_WINDOW_TYPE(NES::Windows::MemoryRegions);
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
    io.IniFilename = layout_file.c_str();
#endif

    cout << "[MyApp] ImGui layout file is " << io.IniFilename << endl;

    // Connect handlers for ImGui to store layout data
    SetupINIHandlers();

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
    style.WindowPadding.x    = 1;
    style.WindowPadding.y    = 1;
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
            if(ImGui::MenuItem("New Project...", "ctrl+o")) {
                auto infos_pane_cb = [=, this](char const* vFilter, IGFDUserDatas vUserDatas, bool* cantContinue) {
                    this->OpenROMInfosPane();
                };

                ImGuiFileDialog::Instance()->OpenDialog("OpenROMFileDialog", "Choose ROM for project", "NES ROMs (*.nes){.nes}", "./roms/", "",
                                                       bind(infos_pane_cb, placeholders::_1, placeholders::_2, placeholders::_3),
                                                       250, 1, IGFDUserDatas("InfosPane"), ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_CaseInsensitiveExtention
                                                                                           | ImGuiFileDialogFlags_DisableCreateDirectoryButton);
            }

            if(ImGui::MenuItem("Open Project...", "ctrl+o")) {
                ImGuiFileDialog::Instance()->OpenDialog("OpenProjectFileDialog", "Open Project", "Project Files (*.rdsproj){.rdsproj}", "./roms/", "", 
                                                       1, nullptr, ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ReadOnlyFileNameField);
            }

            bool do_save_as = false;
            if(ImGui::MenuItem("Save Project", "ctrl+s", nullptr, (bool)current_project)) {
                if(project_file_path.size() == 0) do_save_as = true;
                else {
                    popups.save_project.show = true;
                }
            }

            if(do_save_as || ImGui::MenuItem("Save Project As...", "", nullptr, (bool)current_project)) {
                std::string default_file = current_project->GetRomFileName(); // current loaded file name

                // get only the base filename
                auto i = default_file.rfind("/");
                if(i != std::string::npos) {
                    default_file = default_file.substr(i + 1);
                }
                i = default_file.rfind("\\");
                if(i != std::string::npos) {
                    default_file = default_file.substr(i + 1);
                }

                // append .rdsproj
                i = default_file.find(L'.');
                if(i != std::string::npos) {
                    default_file = default_file.substr(0, i);
                }
                default_file = default_file + ".rdsproj";

                ImGuiFileDialog::Instance()->OpenDialog("SaveProjectFileDialog", "Save Project", "Project Files (*.rdsproj){.rdsproj}", "./roms/", default_file.c_str(), 
                                                       1, nullptr, ImGuiFileDialogFlags_Modal | ImGuiFileDialogFlags_ConfirmOverwrite);
            }

            if(ImGui::MenuItem("Close Project", "", nullptr, (bool)current_project)) {
                CloseProject();
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
                    CloseProject();
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
            //!if(ImGui::MenuItem("Debugger")) {
            //!    auto wnd = SNESDebugger::CreateWindow();
            //!    AddWindow(wnd);
            //!}
            //!if(ImGui::MenuItem("Memory")) {
            //!    auto wnd = SNESMemory::CreateWindow();
            //!    AddWindow(wnd);
            //!}
            if(ImGui::MenuItem("Defines")) {
                auto wnd = NES::Windows::Defines::CreateWindow();
                AddWindow(wnd);
            }

            if(ImGui::MenuItem("Labels")) {
                auto wnd = NES::Windows::Labels::CreateWindow();
                AddWindow(wnd);
            }

            if(ImGui::MenuItem("Listing")) {
                auto wnd = NES::Listing::CreateWindow();
                AddWindow(wnd);
            }

            if(ImGui::MenuItem("Memory")) {
                auto wnd = NES::Windows::MemoryRegions::CreateWindow();
                AddWindow(wnd);
            }
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Debug")) {
            if(ImGui::MenuItem("Show ImGui Demo", "ctrl+d")) {
                show_imgui_demo = true;
            }

            if(ImGui::MenuItem("Expressions test", "")) {
				auto node_creator =	Expression().GetNodeCreator();

                string errmsg;
                int errloc;
                make_shared<Expression>()->Set(string("1+2"), errmsg, errloc);
                make_shared<Expression>()->Set(string("1 + 2"), errmsg, errloc);
                make_shared<Expression>()->Set(string("3 * (1 + -5)"), errmsg, errloc);
                make_shared<Expression>()->Set(string("Function(%0010 | $10) << 5"), errmsg, errloc);

                // and an expression that goes through all the nodes:
                auto expr = make_shared<Expression>();
                expr->Set(string("~(+5 << 2 + -20 | $20 * 2 ^ %1010 / 2 & 200 >> (3 + !0) - 10 **3), Func(two, 3)"), errmsg, errloc);

                // evaluate that first element
                if(auto list = dynamic_pointer_cast<BaseExpressionNodes::ExpressionList>(expr->GetRoot())) {
                    auto node = list->GetNode(0);
					s64 result;
                    string errmsg;
                    if(node->Evaluate(&result, errmsg)) {
                        cout << "evaluation: " << result << " hex: " << hex << result << endl;
                    } else {
                        cout << "evaluation failed: " << errmsg << endl;
                    }
                }

                // make some expression errors
                make_shared<Expression>()->Set(string("Function(3(5))"), errmsg, errloc);
                make_shared<Expression>()->Set(string("1 + ?5"), errmsg, errloc);
                make_shared<Expression>()->Set(string("/35"), errmsg, errloc);
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();

        if(ImGuiFileDialog::Instance()->Display("OpenROMFileDialog")) {
            if(ImGuiFileDialog::Instance()->IsOk()) {
                auto selection = ImGuiFileDialog::Instance()->GetSelection();
                if(selection.size() >  0) {
                    CloseProject();
                    string file_path_name = (*selection.begin()).second;
                    CreateNewProject(file_path_name);
                }
            }

            ImGuiFileDialog::Instance()->Close();
        }

        if(ImGuiFileDialog::Instance()->Display("SaveProjectFileDialog")) {
            if(ImGuiFileDialog::Instance()->IsOk()) {
                project_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                popups.save_project.show = true;
            }

            ImGuiFileDialog::Instance()->Close();
        }

        if(ImGuiFileDialog::Instance()->Display("OpenProjectFileDialog")) {
            if(ImGuiFileDialog::Instance()->IsOk()) {
                CloseProject();
                project_file_path = ImGuiFileDialog::Instance()->GetFilePathName();
                popups.load_project.show = true;
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

    // Process all popups here
    RenderPopups();

    // Remove any windows queued for deletion
    ProcessQueuedWindowsForDelete();

    // CLean up style vars
    ImGui::PopStyleVar(1);
}

void MyApp::RenderPopups()
{
    LoadProjectPopup();
    SaveProjectPopup();
}

void MyApp::CloseProject()
{
    // Close all windows
    for(auto& wnd : managed_windows) {
        wnd->CloseWindow();
    }

    // Drop the reference to the project, which should free everything from memory
    current_project = nullptr;
    project_file_path = "";

    // temp
    BaseWindow::ResetWindowIDs();
}

bool MyApp::StartPopup(std::string const& title, bool resizeable)
{
    auto ctitle = title.c_str();
    if(title != current_popup_title) {
        assert(current_popup_title == ""); // shouldn't be opening two popups at once
        current_popup_title = title;
        ImGui::OpenPopup(title.c_str());
    }

    // center the popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // configure flags
    ImGuiWindowFlags popup_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if(!resizeable) popup_flags |= ImGuiWindowFlags_NoResize;

    // no further rendering if the dialog isn't visible
    if(!ImGui::BeginPopupModal(ctitle, nullptr, popup_flags)) return false;

    return true;
}

int MyApp::EndPopup(int ret, bool show_ok, bool show_cancel, bool allow_escape)
{
    ImVec2 button_size(ImGui::GetFontSize() * 5.0f, 0.0f);
    if(show_ok) {
        // if OK is pressed return 1
        if(ImGui::Button("OK", button_size)) ret = 1;
    }

    if(show_cancel) {
        // if Cancel or escape are pressed return -1
        if(show_ok) ImGui::SameLine();
        if(ImGui::Button("Cancel", button_size)) ret = -1;
    }

    if(allow_escape && ImGui::IsKeyPressed(ImGuiKey_Escape)) ret = -1;

    // If the window is closing call CloseCurrentPopup
    if(ret != 0) {
        current_popup_title = "";
        ImGui::CloseCurrentPopup();
    }

    // Always end
    ImGui::EndPopup();
    return ret;
}

bool MyApp::OKPopup(std::string const& title, std::string const& content, bool resizeable)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    ImGui::Text("%s", content.c_str());

    return EndPopup(ret, true, false);
}

int MyApp::InputNamePopup(std::string const& title, std::string const& label, std::string* buffer, bool enter_returns_true, bool resizeable)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    // focus on the text input if nothing else is selected
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

    ImGuiInputTextFlags input_flags = (enter_returns_true) ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
    if(ImGui::InputText(label.c_str(), buffer, input_flags)) ret = 1; // enter was pressed

    return EndPopup(ret);
}

int MyApp::InputHexPopup(std::string const& title, std::string const& label, std::string* buffer, bool enter_returns_true, bool resizeable)
{
    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    // focus on the text input if nothing else is selected
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

    int ret = 0;
    ImGuiInputTextFlags input_flags = (enter_returns_true) ? ImGuiInputTextFlags_EnterReturnsTrue : 0;
    input_flags |= ImGuiInputTextFlags_CharsHexadecimal;
    if(ImGui::InputText(label.c_str(), buffer, input_flags)) ret = 1; // enter was pressed

    return EndPopup(ret);
}

int MyApp::InputMultilinePopup(std::string const& title, std::string const& label, std::string* buffer, bool resizeable)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    // focus on the text input if nothing else is selected
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

    ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_AllowTabInput;
    ImGui::InputTextMultiline(label.c_str(), buffer, ImVec2(0, 0), input_flags);

    // allow ctrl+enter to close the multiline
    if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Enter)) ret = 1;

    return EndPopup(ret);
}

int MyApp::WaitPopup(std::string const& title, std::string const& content, bool done, bool cancelable, bool resizeable)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    // Show the content of the dialog
    ImGui::Text("%s", content.c_str());

    if(done) ret = 1;

    return EndPopup(ret, false, cancelable, false);
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

    if(current_project) {
        assert(false); // TODO prompt user to close project and current_project->Close();
    }

    // TODO: close the current project and open windows
    // TODO move close windows to project
    for(auto& window : managed_windows) {
        window->CloseWindow();
    }

    auto creator = ProjectCreatorWindow::CreateWindow(file_path_name);
    *creator->project_created += std::bind(&MyApp::ProjectCreatedHandler, this, placeholders::_1, placeholders::_2);
    AddWindow(creator);
}

void MyApp::ProjectCreatedHandler(std::shared_ptr<BaseWindow> project_creator_window, std::shared_ptr<BaseProject> project)
{
    project_creator_window->CloseWindow();

    // TODO: create the default workspace and focus a source editor to the projects entry point

    current_project = project;
    cout << "[MyApp] new " << project->GetInformation()->full_name << " loaded." << endl;
    //!current_system_changed->emit();

    // create the default workspace for the new system
    current_project->CreateDefaultWorkspace();
}

void MyApp::LoadProjectPopup()
{
    auto title = popups.load_project.title.c_str();

    if(!ImGui::IsPopupOpen(title) && popups.load_project.show) {
        if(!popups.load_project.thread) { // create the load project thread
            popups.load_project.loading = true;
            popups.load_project.errored = false;
            popups.load_project.thread = make_shared<std::thread>(std::bind(&MyApp::LoadProjectThread, this));
            cout << "[MyApp::LoadProjectPopup] started load project thread" << endl;
        }

        ImGui::OpenPopup(title);
        popups.load_project.show = false;

        // center the window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if(ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Loading from %s...", project_file_path.c_str());

        if(!popups.load_project.loading) {
            popups.load_project.thread->join();
            popups.load_project.thread = nullptr;

            // TODO this should go away once the workspace is saved in the project file
            if(!popups.load_project.errored) {
                current_project->CreateDefaultWorkspace();
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    // Show the error message
    if(popups.load_project.errored) {
        ImGui::OpenPopup("Error loading project");
        popups.load_project.errored = false;
    }

    if(ImGui::BeginPopupModal("Error loading project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("An error occurred while loading the project: %s", popups.load_project.errmsg.c_str());
        if(ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void MyApp::SaveProjectPopup()
{
    auto title = popups.save_project.title.c_str();

    if(!ImGui::IsPopupOpen(title) && popups.save_project.show) {
        if(!popups.save_project.thread) { // create the save project thread
            popups.save_project.saving = true;
            popups.save_project.errored = false;
            popups.save_project.thread = make_shared<std::thread>(std::bind(&MyApp::SaveProjectThread, this));
            cout << "[MyApp::SaveProjectPopup] started save project thread" << endl;
        }

        ImGui::OpenPopup(title);
        popups.save_project.show = false;

        // center the window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if(ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Saving to %s...", project_file_path.c_str());

        if(!popups.save_project.saving) {
            popups.save_project.thread->join();
            popups.save_project.thread = nullptr;
            ImGui::CloseCurrentPopup();

        }

        ImGui::EndPopup();
    }

    // Show the error message
    if(popups.save_project.errored) {
        ImGui::OpenPopup("Error saving project");
        popups.save_project.errored = false;
    }

    if(ImGui::BeginPopupModal("Error saving project", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("An error occurred while saving the project: %s", popups.save_project.errmsg.c_str());
        if(ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

#define PROJECT_FILE_MAGIC 0x8781a90afde1f317ULL
#define PROJECT_FILE_VERSION 0x00000101
void MyApp::SaveProjectThread()
{
    ofstream out(project_file_path, ios::binary);
    if(out.good()) {
        u64 magic = PROJECT_FILE_MAGIC; 
        u32 version = PROJECT_FILE_VERSION; 
        u32 flags = 0; 

        out.write((char*)&magic, sizeof(magic));
        out.write((char*)&version, sizeof(version));
        out.write((char*)&flags, sizeof(flags));

        if(out.good()) {
            popups.save_project.errored = !current_project->Save(out, popups.save_project.errmsg);
        } else {
            popups.save_project.errored = true;
            popups.save_project.errmsg = "Could not write to file";
        }
    } else {
        popups.save_project.errored = true;
        popups.save_project.errmsg = "Could not open file";
    }
    
    if(out.good()) out.flush();
    if(!popups.save_project.errored) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    popups.save_project.saving = false;
}

void MyApp::LoadProjectThread()
{
    cout << "load thread" << endl;

    ifstream is(project_file_path, ios::binary);
    if(is.good()) {
        u64 magic;
        u32 version;
        u32 flags;

        is.read((char*)&magic, sizeof(magic));
        is.read((char*)&version, sizeof(version));
        is.read((char*)&flags, sizeof(flags));

        if(!is.good()) {
            popups.load_project.errmsg = "Could not read from file";
            popups.load_project.errored = true;
        } else if(magic != PROJECT_FILE_MAGIC) {
            popups.load_project.errmsg = "Not a Retro Disassembly Studio project file";
            popups.load_project.errored = true;
        } else if(version != PROJECT_FILE_VERSION) {
            popups.load_project.errmsg = "The project file contains an invalid version number";
            popups.load_project.errored = true;
        }

        if(!popups.load_project.errored) {
            current_project = BaseProject::LoadProject(is, popups.load_project.errmsg);
            // error condition when the returned object is null
            popups.load_project.errored = !current_project;
        }
    } else {
        popups.load_project.errored = true;
        popups.load_project.errmsg = "Could not open file";
    }
    
    if(!popups.load_project.errored) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    popups.load_project.loading = false;
}


int main(int argc, char* argv[])
{
    return MyApp::Instance(argc, argv)->Run();
}
