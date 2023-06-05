// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "imgui.h"
#include "imgui_stdlib.h"

#include "util.h"

#include "systems/nes/cartridge.h"
#include "systems/nes/system.h"

#include "windows/nes/defines.h"
#include "windows/nes/emulator.h"
#include "windows/nes/listing.h"
#include "windows/nes/project.h"

using namespace std;

namespace Windows::NES {

Windows::BaseProject::Information const* Project::GetInformation()
{
    return Project::GetInformationStatic();
}

Windows::BaseProject::Information const* Project::GetInformationStatic()
{
    static Project::Information information = {
        .abbreviation   = "NES",
        .full_name      = "Nintendo Entertainment System",
        .is_rom_valid   = std::bind(&Project::IsROMValid, placeholders::_1, placeholders::_2),
        .create_project = std::bind(&Project::CreateProject),
    };

    return &information;
}

shared_ptr<Windows::BaseProject> Project::CreateProject()
{
    return make_shared<Project>();
}

Project::Project()
    : Windows::BaseProject("Project")
{
}

Project::~Project()
{
}

bool Project::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    unsigned char buf[16];
    is.read(reinterpret_cast<char*>(buf), 16);
    if(is && (buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        return true;
    }

    return false;
}

bool Project::CreateNewProjectFromFile(string const& file_path_name)
{
    cout << "[NES::Project] CreateNewProjectFromFile begin" << endl;
    rom_file_name = file_path_name;

    // create a barebones system with nothing loaded
    auto system = make_shared<System>();
    current_system = system;

    // Before we can read ROM, we need a place to store it
    system->CreateMemoryRegions();

    auto selfptr = dynamic_pointer_cast<Project>(shared_from_this());
    create_new_project_progress->emit(selfptr, false, 0, 0, "Loading file...");

    // Read in the iNES header
    ifstream rom_stream(file_path_name, ios::binary);
    if(!rom_stream) {
        create_new_project_progress->emit(selfptr, true, 0, 0, "Error: Could not open file");
        return false;
    }

    unsigned char buf[16];
    rom_stream.read(reinterpret_cast<char*>(buf), 16);
    if(!rom_stream || !(buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        create_new_project_progress->emit(selfptr, true, 0, 0, "Error: Not an NES ROM file");
        return false;
    }
 
    // configure the cartridge memory
    auto cartridge = system->GetCartridge();
    cartridge->LoadHeader(buf);

    // skip the trainer if present
    if(cartridge->header.has_trainer) rom_stream.seekg(512, rom_stream.cur);

    // we now know how many things we need to load
    u32 num_steps = cartridge->header.num_prg_rom_banks + cartridge->header.num_chr_rom_banks + 1;
    u32 current_step = 0;

    // Load the PRG banks
    for(u32 i = 0; i < cartridge->header.num_prg_rom_banks; i++) {
        stringstream ss;
        ss << "Loading PRG ROM bank " << i;
        create_new_project_progress->emit(selfptr, false, num_steps, ++current_step, ss.str());

        // Read in the PRG rom data
        unsigned char data[16 * 1024];
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(selfptr, true, num_steps, current_step, "Error: file too short when reading PRG-ROM");
            return false;
        }

        // Get the PRG-ROM bank
        auto prg_bank = cartridge->GetProgramRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        prg_bank->InitializeFromData(reinterpret_cast<u8*>(data), sizeof(data)); // Initialize the memory region with bytes of data
    }

    // Load the CHR banks
    for(u32 i = 0; i < cartridge->GetNumCharacterRomBanks(); i++) {
        stringstream ss;
        ss << "Loading CHR ROM bank " << i;
        create_new_project_progress->emit(selfptr, false, num_steps, ++current_step, ss.str());

        // Get the CHR bank
        auto chr_bank = cartridge->GetCharacterRomBank(i); // Bank should be empty with no content

        // Read in the CHR rom data
        unsigned char data[8 * 1024]; // max bank size is 8K but we may read 4K banks
        assert(chr_bank->GetRegionSize() <= 8*1024);
        rom_stream.read(reinterpret_cast<char *>(data), chr_bank->GetRegionSize());
        if(!rom_stream) {
            create_new_project_progress->emit(selfptr, true, num_steps, current_step, "Error: file too short when reading CHR-ROM");
            return false;
        }

        // Initialize the entire bank as just a series of bytes
        chr_bank->InitializeFromData(reinterpret_cast<u8*>(data), chr_bank->GetRegionSize()); // Mark content starting at offset 0 as data
    }

    // create labels for reset and the registers, etc
    system->CreateDefaultDefines();
    system->CreateDefaultLabels();

    create_new_project_progress->emit(selfptr, false, num_steps, ++current_step, "Done");
    this_thread::sleep_for(std::chrono::seconds(1));

    cout << "[NES::Project] CreateNewProjectFromFile end" << endl;
    return true;
}

void Project::CreateSystemInstance()
{
    auto system_instance = Windows::NES::SystemInstance::CreateWindow();
    system_instance->SetInitialDock(BaseWindow::DOCK_ROOT);
    AddChildWindow(system_instance);
    system_instance->CreateDefaultWorkspace();
}

bool Project::Save(std::ostream& os, std::string& errmsg) 
{
    // call the base method first to inject the project Information
    if(!Windows::BaseProject::Save(os, errmsg)) return false;

    // save the System structure
    auto system = GetSystem<System>();
    if(!system->Save(os, errmsg)) return false;

    return true;
}

bool Project::Load(std::istream& is, std::string& errmsg)
{
    if(!Windows::BaseProject::Load(is, errmsg)) return false;

    current_system = make_shared<System>();
    auto system = GetSystem<System>();
    if(!system->Load(is, errmsg)) return false;

    return true;
}

void Project::Update(double deltaTime) 
{
}

void Project::Render() 
{
    RenderPopups();
}

bool Project::StartPopup(string const& title, bool resizeable)
{
    auto ctitle = title.c_str();
    if(title != popups.current_title) {
        assert(popups.current_title == ""); // shouldn't be opening two popups at once
        popups.current_title = title;
        ImGui::OpenPopup(title.c_str());
    }

    // center the popup
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    // configure flags
    ImGuiWindowFlags popup_flags = 0;
    if(!resizeable) popup_flags |= ImGuiWindowFlags_NoResize;

    // no further rendering if the dialog isn't visible
    if(!ImGui::BeginPopupModal(ctitle, nullptr, popup_flags)) return false;

    return true;
}

int Project::EndPopup(int ret, bool show_ok, bool show_cancel, bool allow_escape)
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
        popups.current_title = "";
        ImGui::CloseCurrentPopup();
    }

    // Always end
    ImGui::EndPopup();
    return ret;
}


void Project::RenderPopups()
{
    if(popups.create_new_define.show) RenderCreateNewDefinePopup();
}

void Project::RenderCreateNewDefinePopup()
{
    int ret = 0;

    // Do the OKPopup instead
    if(popups.ok.show) {
        if(GetMainWindow()->OKPopup(popups.ok.title, popups.ok.content)) {
            // return to editing the expression
            popups.ok.show = false;
        }

        return;
    }

    // no further rendering if the dialog isn't visible
    if(!StartPopup(popups.create_new_define.title, true)) return;

    if(popups.create_new_define.focus) {
        ImGui::SetKeyboardFocusHere();
        popups.create_new_define.focus = false;
    }
    ImGui::InputText("Name", &popups.buffer1, 0);
    ImGui::SetItemDefaultFocus();

    if(ImGui::InputText("Expression", &popups.buffer2, ImGuiInputTextFlags_EnterReturnsTrue)) ret = 1; // enter was pressed

    if((ret = EndPopup(ret)) != 0) {
        if(ret > 0) {
            cout << "creating define " << popups.buffer1 << " expr " << popups.buffer2 << endl;

            // Try creating the define
            if(auto system = GetSystem<System>()) {
                string errmsg;
                if(!system->AddDefine(popups.buffer1, popups.buffer2, errmsg)) {
                    //cout << "could not create define: "<< errmsg << endl;
                    popups.ok.title = "Expression";
                    popups.ok.content = string("Error creating expression: ") + errmsg;
                    popups.ok.show = true;
                } else {
                    popups.create_new_define.show = false;
                }
            }
        } else {
            popups.create_new_define.show = false;
        }
    }
}

void Project::ChildWindowAdded(std::shared_ptr<BaseWindow> const& window)
{
    if(auto wnd = dynamic_pointer_cast<Windows::NES::Defines>(window)) {
        *wnd->command_signal += std::bind(&Project::CommonCommandHandler, this, placeholders::_1, placeholders::_2, placeholders::_3);
    } else if(auto system_instance = dynamic_pointer_cast<Windows::NES::SystemInstance>(window)) {
        *window->window_activated += [this](shared_ptr<BaseWindow> const& _wnd) {
            most_recent_system_instance = _wnd;
        };
    }
}

void Project::ChildWindowRemoved(std::shared_ptr<BaseWindow> const& window)
{
    if(window == most_recent_system_instance) {
        most_recent_system_instance = nullptr;
    }
}

void Project::CommonCommandHandler(shared_ptr<BaseWindow>& wnd, string const& command, void* userdata)
{
    auto system = GetSystem<System>();

    if(command == "CreateNewDefine") {
        CreateNewDefineData* data = (CreateNewDefineData*)userdata;
        popups.create_new_define.focus = true;
        popups.create_new_define.show = true;
        popups.buffer1 = "";
        popups.buffer2 = "";
    }
}

}
