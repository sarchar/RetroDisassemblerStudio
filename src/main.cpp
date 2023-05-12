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

#define USE_IMGUI_TABLES
#include "ImGuiFileDialog.h"
#include "dirent/dirent.h"

#include "main.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"
#include "windows/rom_loader.h"
#include "windows/nes/listing.h"
#include "windows/nes/listing.h"

#include "systems/expressions.h" // TODO temp

#undef DISABLE_IMGUI_SAVE_LOAD_LAYOUT

using namespace std;

MyApp::MyApp(int argc, char* argv[])
    : Application("Retro Disassembler Studio", 1600, 1000),
      request_exit(false), show_imgui_demo(false)
{
    ((void)argc);
    ((void)argv);
    ProjectCreatorWindow::RegisterProjectInformation(NES::Project::GetInformationStatic());
//!    current_system_changed = make_shared<current_system_changed_t>();

    // register all the windows
#   define REGISTER_WINDOW_TYPE(className) \
        create_window_functions[className::GetWindowClassStatic()] = std::bind(&className::CreateWindow);
    REGISTER_WINDOW_TYPE(NES::Listing);
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
    style.WindowPadding.x    = 4;
    style.WindowPadding.y    = 4;
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
    // TODO HACK ALERT! need a clearly way of catching these signals
    if(auto listing = dynamic_pointer_cast<NES::Listing>(window)) {
        *listing->listing_command += std::bind(&MyApp::ListingWindowCommand, this, placeholders::_1, placeholders::_2, placeholders::_3);
    }
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

            if(ImGui::MenuItem("Save Project...", "ctrl+s")) {
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
                                                       1, nullptr, ImGuiFileDialogFlags_Modal);
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
            //!if(ImGui::MenuItem("Debugger")) {
            //!    auto wnd = SNESDebugger::CreateWindow();
            //!    AddWindow(wnd);
            //!}
            //!if(ImGui::MenuItem("Memory")) {
            //!    auto wnd = SNESMemory::CreateWindow();
            //!    AddWindow(wnd);
            //!}
            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Debug")) {
            if(ImGui::MenuItem("Show ImGui Demo", "ctrl+d")) {
                show_imgui_demo = true;
            }

            if(ImGui::MenuItem("Expressions test", "")) {
                // create an expression of the form "Label-1"
                shared_ptr<BaseExpression> expr = make_shared<Expression>();
                auto node_creator = expr->GetNodeCreator();

                // Try to fully represent the following:
                // 3 * ((1 + myFunc)-( $10*otherFunc ))
                // and have it should evaluate to -126

                auto constant_one = node_creator->CreateConstantU8(1, "1");
                auto label = node_creator->CreateName("myFunc");
                auto add = node_creator->CreateAddOp(constant_one, label, " + ");
                auto paren1 = node_creator->CreateParens("(", add, ")");
                auto constant_two = node_creator->CreateConstantU8(0x10, "$10");
                auto label2 = node_creator->CreateName("otherFunc");
                auto mul = node_creator->CreateMultiplyOp(constant_two, label2, "*");
                auto paren2 = node_creator->CreateParens("( ", mul, " )");
                auto sub = node_creator->CreateSubtractOp(paren1, paren2, "-");
                auto paren3 = node_creator->CreateParens("(", sub, ")");
                auto constant_three = node_creator->CreateConstantU8(3, "3");
                auto mul2 = node_creator->CreateMultiplyOp(constant_three, paren3, " * ");
                expr->Set(mul2);

                class ExpressionHelper : public BaseExpressionHelper {
                public:
                    ExpressionHelper(shared_ptr<BaseExpressionNodeCreator>& nc) 
                        : node_creator(nc) {}

                    shared_ptr<BaseExpressionNode> ResolveName(string const& name) override {
                        if(name == "myFunc") return node_creator->CreateConstantU8(5, "5");
                        if(name == "otherFunc") return node_creator->CreateConstantU8(3, "3");
                        return nullptr;
                    }

                    shared_ptr<BaseExpressionNodeCreator> node_creator;
                };

                shared_ptr<ExpressionHelper> helper = make_shared<ExpressionHelper>(node_creator);

                // and print it
                cout << "made expression: " << *expr << endl;
                s64 result;
                if(expr->Evaluate(helper, &result)) {
                    cout << "evaluated: " << result << endl;
                } else {
                    cout << "failed evaluation" << endl;
                }
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

        // overwrite existing project?
        {
            if (ImGui::BeginPopupModal("OverwriteExitingProject", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::Text("The file %s already exists. Overwrite?", project_file_path.c_str());
                
                if(ImGui::Button("Yes")) {
                    ImGui::CloseCurrentPopup();
                    ImGuiFileDialog::Instance()->Close(); // close file dialog too
                    popups.save_project.show = true;
                }

                ImGui::SameLine();
                if(ImGui::Button("No")) {
                    ImGui::CloseCurrentPopup();
                    // don't close ImGuiFileDialog, keep asking
                }

                ImGui::EndPopup();
            }
        } 

        if(!ImGui::IsPopupOpen("OverwriteExitingProject") && ImGuiFileDialog::Instance()->Display("SaveProjectFileDialog")) {
            bool close_file_dialog = true;

            if(ImGuiFileDialog::Instance()->IsOk()) {
                project_file_path = ImGuiFileDialog::Instance()->GetFilePathName();

                // if the file exists, ask the user to overwrite
                if(std::filesystem::exists(project_file_path)) {
                    close_file_dialog = false; // don't close file dialog yet
                    ImGui::OpenPopup("OverwriteExitingProject");
                } else {
                    popups.save_project.show = true;
                }
            }

            if(close_file_dialog) ImGuiFileDialog::Instance()->Close();
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
    if(current_project) {
        CreateLabelPopup();
        DisassemblyPopup();
        GoToAddressPopup();
    }

    SaveProjectPopup();
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

void MyApp::ListingWindowCommand(shared_ptr<BaseWindow> const& wnd, string const& cmd, NES::GlobalMemoryLocation const& where)
{
    // TODO this should be generic
    if(auto system = current_project->GetSystem<NES::System>()) {
        if(cmd == "CreateLabel") {
            popups.create_label.uhg = make_shared<NES::GlobalMemoryLocation>(where);
            popups.create_label.show = true;

        } else if(cmd == "DisassemblyRequested") {
            // if currently disassembling, ignore the request
            if(popups.disassembly.thread) return;
        
            system->InitDisassembly(where);
            popups.disassembly.show = true;
        } else if(cmd == "GoToAddress") {
            popups.goto_address.listing = wnd;
            popups.goto_address.show = true;
        }
    }
}

void MyApp::CreateLabelPopup() 
{
    auto title = popups.create_label.title.c_str();
    bool should_close = false;

    // TODO this should be generic
    auto system = current_project->GetSystem<NES::System>();
    if(!system) return;

    if(!ImGui::IsPopupOpen(title) && popups.create_label.show) {
        popups.create_label.show = false;
        popups.create_label.buf[0] = '\0';
        popups.create_label.edit = -1;

        NES::GlobalMemoryLocation* where = static_cast<NES::GlobalMemoryLocation*>(popups.create_label.uhg.get());
        auto labels = system->GetLabelsAt(*where);
        if(labels.size()) {
            int nth = 0;
            strncpy(popups.create_label.buf, labels.at(nth)->GetString().c_str(), sizeof(popups.create_label.buf));
            popups.create_label.edit = nth;
        }

        ImGui::OpenPopup(title);
    }

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // return if the dialog isn't open
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    // focus on the text input if nothing else is selected
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

    // if either Enter is pressed or OK clicked, create the label
    ImVec2 button_size(ImGui::GetFontSize() * 7.0f, 0.0f);
    if(ImGui::InputText("Label", popups.create_label.buf, sizeof(popups.create_label.buf), ImGuiInputTextFlags_EnterReturnsTrue)
       || ImGui::Button("OK", button_size)) {
        string lstr(popups.create_label.buf);
        NES::GlobalMemoryLocation* where = static_cast<NES::GlobalMemoryLocation*>(popups.create_label.uhg.get());

        if(ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || popups.create_label.edit == -1) {
            // add the label
            system->CreateLabel(*where, lstr, true);
        } else {
            // replace the label
            system->EditLabel(*where, lstr, 0, true);
        }

        should_close = true;
    }

    if(should_close || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


void MyApp::DisassemblyPopup()
{
    auto title = popups.disassembly.title.c_str();
    
    // TODO this should be generic and we can use current_system->DisassemblyThread
    auto system = current_project->GetSystem<NES::System>();
    if(!system) return;

    // Check if the dialog needs to be opened
    if(!ImGui::IsPopupOpen(title) && popups.disassembly.show) {
        ImGui::OpenPopup(title);
        popups.disassembly.show = false;

        // create the disassembly thread
        popups.disassembly.thread = make_unique<std::thread>(std::bind(&NES::System::DisassemblyThread, system));
        cout << "[Listing::DisassemblyPopup] started disassembly thread" << endl;
    }

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // Return if the dialog isn't open
    ImGuiWindowFlags popup_flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings; // TODO generate better flags
    if(!ImGui::BeginPopupModal(title, nullptr, popup_flags)) return;

    // Render the content of the dialog
    ImGui::Text("Disassembling...");

    // Check if the disassembly is done
    if(!system->IsDisassembling()) {
        if(popups.disassembly.thread) {
            popups.disassembly.thread->join();
            popups.disassembly.thread = nullptr;
            cout << "[Listing::DisassemblyPopup] disassembly thread exited" << endl;
        }
        
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void MyApp::GoToAddressPopup() 
{
    auto title = popups.goto_address.title.c_str();
    bool should_close = false;

    // TODO this should be generic and we can use current_system->DisassemblyThread
    auto system = current_project->GetSystem<NES::System>();
    if(!system) return;

    if(!ImGui::IsPopupOpen(title) && popups.goto_address.show) {
        popups.goto_address.show = false;

        popups.goto_address.buf[0] = '\0';

        ImGui::OpenPopup(title);
    }

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // return if the dialog isn't open
    if (!ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    // focus on the text input if nothing else is selected
    if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

    // if either Enter is pressed or OK clicked, create the label
    // TODO allow expressions
    ImVec2 button_size(ImGui::GetFontSize() * 7.0f, 0.0f);
    if(ImGui::InputText("Address (hex)", popups.goto_address.buf, sizeof(popups.goto_address.buf), 
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal) || ImGui::Button("OK", button_size)) {

        // parse the address given
        stringstream ss;
        ss << hex << popups.goto_address.buf;
        u32 address;
        ss >> address;

        // let Listing do the heavy work of interpreting the address and which bank it would be in
        // call listing directly rather than use a signal, since it's not really a global event -- it's specific to the
        // listing window that requested the goto address
        shared_ptr<NES::Listing> listing = dynamic_pointer_cast<NES::Listing>(popups.goto_address.listing);
        if(listing) listing->GoToAddress(address);

        should_close = true;
    }

    if(should_close || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
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

void MyApp::SaveProjectThread()
{
    cout << "save thread" << endl;

    ofstream out(project_file_path, ios::binary);
    if(out.good()) {
        u64 magic = 0x8781a90afde1f317; 
        u32 flags = 0; 
        u32 version = 0; 

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

int main(int argc, char* argv[])
{
    return MyApp::Instance(argc, argv)->Run();
}
