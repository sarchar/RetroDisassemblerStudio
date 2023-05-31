#include <iostream>
#include <memory>
#include <sstream>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#define USE_IMGUI_TABLES
#include "ImGuiFileDialog.h"
#include "dirent/dirent.h"

#include "../main_application.h"

#include "windows/baseproject.h"
#include "windows/main.h"
#include "windows/rom_loader.h"
#include "windows/nes/defines.h"
#include "windows/nes/emulator.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
#include "windows/nes/regions.h"

using namespace std;

namespace Windows {

std::shared_ptr<MainWindow> MainWindow::CreateWindow()
{
    return make_shared<MainWindow>();
}

MainWindow::MainWindow()
    : BaseWindow("Windows::MainWindow"), show_imgui_demo(false)
{
    SetTitle("Retro Disassembler Studio");

    // disable frame, resize, etc
    SetMainWindow(true);

    // make this window dockable-into
    SetIsDockSpace(true);

    // and we can't be docked into other things
    SetDockable(false);

    // show a menu bar
    SetShowMenuBar(true);

    // show a status bar
    SetShowStatusBar(true);

    *child_window_added += std::bind(&MainWindow::ChildWindowAdded, this, placeholders::_1);
    *child_window_removed += std::bind(&MainWindow::ChildWindowRemoved, this, placeholders::_1);
}

MainWindow::~MainWindow()
{
}

void MainWindow::ChildWindowAdded(std::shared_ptr<BaseWindow> const& window)
{
    if(auto system_instance = dynamic_pointer_cast<Windows::NES::SystemInstance>(window)) {
        *window->window_activated += [this](shared_ptr<BaseWindow> const& _wnd) {
            most_recent_system_instance = _wnd;
        };
    }
}

void MainWindow::ChildWindowRemoved(std::shared_ptr<BaseWindow> const& window)
{
    if(window == most_recent_system_instance) {
        most_recent_system_instance = nullptr;
    }
}

void MainWindow::Update(double deltaTime)
{
}

void MainWindow::CheckInput()
{
}

void MainWindow::PreRender()
{
}

void MainWindow::Render()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    // Show the ImGui demo window if requested
    if(show_imgui_demo) ImGui::ShowDemoWindow(&show_imgui_demo);

    // Process all popups here
    RenderPopups();

    // CLean up style vars
    ImGui::PopStyleVar(1);

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

void MainWindow::PostRender()
{
//    ImGui::PopStyleVar(3);
}

void MainWindow::RenderMenuBar()
{
    //if(!ImGui::BeginMainMenuBar()) return;

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
            command_signal->emit(shared_from_this(), "RequestExit", nullptr);
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
                    if(ends_with(strlower(file_path_name), ".nes")) {
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

    if(current_project) {
        if(ImGui::BeginMenu("Windows")) {
            if(ImGui::MenuItem("New Instance")) {
                current_project->CreateSystemInstance();
            }

            if(auto si = dynamic_pointer_cast<Windows::NES::SystemInstance>(most_recent_system_instance)) {
                if(ImGui::BeginMenu("Instance")) {
                    static char const * const window_types[] = {
                        "Defines", "Labels", "Listing", "Memory", "Screen"
                    };

                    for(int i = 0; i < IM_ARRAYSIZE(window_types); i++) {
                        if(ImGui::MenuItem(window_types[i])) {
                            si->CreateNewWindow(window_types[i]);
                        }
                    }

                    ImGui::EndMenu();
                }
            }

            ImGui::EndMenu();
        }
    }

    if(ImGui::BeginMenu("Debug")) {
        if(ImGui::MenuItem("Show ImGui Demo", "ctrl+d")) {
            show_imgui_demo = true;
        }

        //!if(ImGui::MenuItem("Expressions test", "")) {
        //!    auto node_creator =	Expression().GetNodeCreator();

        //!    string errmsg;
        //!    int errloc;
        //!    make_shared<Expression>()->Set(string("1+2"), errmsg, errloc);
        //!    make_shared<Expression>()->Set(string("1 + 2"), errmsg, errloc);
        //!    make_shared<Expression>()->Set(string("3 * (1 + -5)"), errmsg, errloc);
        //!    make_shared<Expression>()->Set(string("Function(%0010 | $10) << 5"), errmsg, errloc);

        //!    // and an expression that goes through all the nodes:
        //!    auto expr = make_shared<Expression>();
        //!    expr->Set(string("~(+5 << 2 + -20 | $20 * 2 ^ %1010 / 2 & 200 >> (3 + !0) - 10 **3), Func(two, 3)"), errmsg, errloc);

        //!    // evaluate that first element
        //!    if(auto list = dynamic_pointer_cast<BaseExpressionNodes::ExpressionList>(expr->GetRoot())) {
        //!        auto node = list->GetNode(0);
        //!        s64 result;
        //!        string errmsg;
        //!        if(node->Evaluate(&result, errmsg)) {
        //!            cout << "evaluation: " << result << " hex: " << hex << result << endl;
        //!        } else {
        //!            cout << "evaluation failed: " << errmsg << endl;
        //!        }
        //!    }

        //!    // make some expression errors
        //!    make_shared<Expression>()->Set(string("Function(3(5))"), errmsg, errloc);
        //!    make_shared<Expression>()->Set(string("1 + ?5"), errmsg, errloc);
        //!    make_shared<Expression>()->Set(string("/35"), errmsg, errloc);
        //!}
        ImGui::EndMenu();
    }

    //ImGui::EndMainMenuBar();

}

void MainWindow::OpenROMInfosPane()
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

    ImGui::PushFont(static_cast<ImFont*>(GetApplication()->GetBoldFont()));
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

void MainWindow::CreateNewProject(string const& file_path_name)
{
    cout << WindowPrefix() << "CreateNewProject(" << file_path_name << ")" << endl;

    if(current_project) {
        // TODO prompt user to close project and current_project->Close();
        CloseProject();
    }

    CloseChildWindows();

    auto creator = Windows::ProjectCreatorWindow::CreateWindow(file_path_name);
    *creator->project_created += std::bind(&MainWindow::ProjectCreatedHandler, this, placeholders::_1, placeholders::_2);
    AddChildWindow(creator);
}

void MainWindow::ProjectCreatedHandler(std::shared_ptr<BaseWindow> project_creator_window, std::shared_ptr<BaseProject> project)
{
    project_creator_window->CloseWindow();

    current_project = project;
    cout << WindowPrefix() << "New " << project->GetInformation()->full_name << " loaded." << endl;
    //!current_system_changed->emit();

    AddChildWindow(project);

    // create the default workspace for the new system
    current_project->CreateSystemInstance();
}


void MainWindow::CloseProject()
{
    // Close all child windows TODO only relating to current project
    CloseChildWindows();

    // Drop the reference to the project, which should free everything from memory
    current_project = nullptr;
    project_file_path = "";

    // temp
    BaseWindow::ResetWindowIDs();
}


void MainWindow::RenderStatusBar() 
{
    ImGui::Text("Main Window status bar");
}

bool MainWindow::StartPopup(std::string const& title, bool resizeable, bool always_centered)
{
    auto ctitle = title.c_str();
    if(title != current_popup_title) {
        assert(current_popup_title == ""); // shouldn't be opening two popups at once
        current_popup_title = title;
        ImGui::OpenPopup(title.c_str());
    }

    // center the popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, always_centered ? ImGuiCond_Always : ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // configure flags
    ImGuiWindowFlags popup_flags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if(!resizeable) popup_flags |= ImGuiWindowFlags_NoResize;

    // no further rendering if the dialog isn't visible
    if(!ImGui::BeginPopupModal(ctitle, nullptr, popup_flags)) return false;

    return true;
}

int MainWindow::EndPopup(int ret, bool show_ok, bool show_cancel, bool allow_escape, bool focus_ok)
{
    ImVec2 button_size(ImGui::GetFontSize() * 5.0f, 0.0f);
    if(show_ok) {
        // if OK is pressed return 1
        if(ImGui::Button("OK", button_size)) ret = 1;

        if(focus_ok && !ImGui::IsAnyItemFocused()) ImGui::SetKeyboardFocusHere(-1);
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

bool MainWindow::OKPopup(std::string const& title, std::string const& content, bool resizeable)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable)) return 0;

    ImGui::Text("%s", content.c_str());

    // Give the first button focus
    return EndPopup(ret, true, false, true, true);
}

int MainWindow::InputNamePopup(std::string const& title, std::string const& label, std::string* buffer, bool enter_returns_true, bool resizeable)
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

int MainWindow::InputHexPopup(std::string const& title, std::string const& label, std::string* buffer, bool enter_returns_true, bool resizeable)
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

int MainWindow::InputMultilinePopup(std::string const& title, std::string const& label, std::string* buffer, bool resizeable)
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

int MainWindow::WaitPopup(std::string const& title, std::string const& content, bool done, bool cancelable, bool resizeable, bool wait_ok)
{
    int ret = 0;

    // no further rendering if the dialog isn't visible
    if(!StartPopup(title, resizeable, true)) return 0;

    // Show the content of the dialog
    ImGui::Text("%s", content.c_str());

    if(done) {
        if(!wait_ok || (wait_ok && ImGui::Button("OK"))) {
            ret = 1;
        }
    }

    return EndPopup(ret, false, cancelable, false);
}


void MainWindow::RenderPopups()
{
    LoadProjectPopup();
    SaveProjectPopup();
}

void MainWindow::LoadProjectPopup()
{
    auto title = popups.load_project.title.c_str();

    if(!ImGui::IsPopupOpen(title) && popups.load_project.show) {
        if(!popups.load_project.thread) { // create the load project thread
            popups.load_project.loading = true;
            popups.load_project.errored = false;
            popups.load_project.thread = make_shared<std::thread>(std::bind(&MainWindow::LoadProjectThread, this));
            cout << "[MainWindow::LoadProjectPopup] started load project thread" << endl;
        }

        ImGui::OpenPopup(title);
        popups.load_project.show = false;

        // center the window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    }

    if(ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Loading from %s...", project_file_path.c_str());

        if(!popups.load_project.loading) {
            popups.load_project.thread->join();
            popups.load_project.thread = nullptr;

            // TODO this should go away once the workspace is saved in the project file
            if(!popups.load_project.errored) {
                AddChildWindow(current_project);
                current_project->CreateSystemInstance();
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

void MainWindow::SaveProjectPopup()
{
    auto title = popups.save_project.title.c_str();

    if(!ImGui::IsPopupOpen(title) && popups.save_project.show) {
        if(!popups.save_project.thread) { // create the save project thread
            popups.save_project.saving = true;
            popups.save_project.errored = false;
            popups.save_project.thread = make_shared<std::thread>(std::bind(&MainWindow::SaveProjectThread, this));
            cout << "[MainWindow::SaveProjectPopup] started save project thread" << endl;
        }

        ImGui::OpenPopup(title);
        popups.save_project.show = false;

        // center the window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
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
void MainWindow::SaveProjectThread()
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

void MainWindow::LoadProjectThread()
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
            current_project = BaseProject::StartLoadProject(is, popups.load_project.errmsg);
            if(current_project && !current_project->Load(is, popups.load_project.errmsg)) {
                current_project = nullptr;
            }
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



}
