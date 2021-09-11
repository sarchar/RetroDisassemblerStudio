#include <fstream>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "rom_loader.h"
#include "signals.h"
#include "systems/system.h"

using namespace std;

std::vector<System::Information const*> ROMLoader::system_informations;

shared_ptr<ROMLoader> ROMLoader::CreateWindow(string const& _file_path_name)
{
    return make_shared<ROMLoader>(_file_path_name);
}

ROMLoader::ROMLoader(string const& _file_path_name)
    : BaseWindow("ROM Loader"),
      file_path_name(_file_path_name),
      loader_state(LOADER_STATE_INIT)
{
    SetWindowless(true);
    system_loaded = make_shared<system_loaded_t>();
}

ROMLoader::~ROMLoader()
{
}

void ROMLoader::UpdateContent(double deltaTime) 
{
    // loop over all systems asking if the file name is valid
    // if there's only 1 valid system, load it
    // if there's more than 1, ask the user to clarify which system should load it
    // returns a System instance
    switch(loader_state) {
    case LOADER_STATE_INIT:
    {
        // open the rom file
        ifstream is(file_path_name, ios::binary);
        if(!is) {
            loader_state = LOADER_STATE_FILE_NOT_FOUND;
            break;
        }

        // loop over all the loaders and accumuilate the valid ones
        vector<System::Information const*> valid_systems;
        for(auto info : ROMLoader::system_informations) {
            if(info->is_rom_valid(file_path_name, is)) {
                valid_systems.push_back(info);
            }

            // rewind the pointer for the next call
            is.clear();
            is.seekg(0);
        }

        // if there are no valid loaders, tell the user
        if(valid_systems.size() == 0) {
            loader_state = LOADER_STATE_NOT_A_VALID_ROM;
            break;
        }

        // if there's only 1 valid loader, load it
        if(valid_systems.size() == 1) {
            CreateSystem(valid_systems[0]);
            CloseWindow();
            break;
        }
        
        // otherwise, allow the user to select which system to load
        available_systems = valid_systems;
        loader_state = LOADER_STATE_SELECT_SYSTEM;
        break;
    }
    default:
        break;
    }
}

void ROMLoader::RenderContent() 
{
    switch(loader_state) {
    case LOADER_STATE_NOT_A_VALID_ROM:
        if(MyApp::Instance()->OKPopup("ROM Loader##notvalid", "The selected ROM file is not valid with any supported retro system.")) {
            CloseWindow();
        }
        break;
    case LOADER_STATE_FILE_NOT_FOUND:
        if(MyApp::Instance()->OKPopup("ROM Loader##notfound", "The selected ROM file was not found or could not be opened for reading.")) {
            CloseWindow();
        }
        break;
    case LOADER_STATE_SELECT_SYSTEM:
    {
        static int current_selection = 0;

        // Always open it
        string title = "ROM Loader - Select System";
        ImGui::OpenPopup(title.c_str());

        // center this window
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoResize;
        if(ImGui::BeginPopupModal(title.c_str(), nullptr, flags)) {
            //TextWrapped doesn't work well
            //ImGui::PushTextWrapPos(ImGui::GetContentRegionAvailWidth());
            ImGui::Text("Multiple systems appear valid. Select which system to use to load the ROM.");
            for(int i = 0; i < available_systems.size(); ++i) {
                auto info = available_systems[i];
                stringstream ss;
                ss << (i + 1) << ". " << info->full_name;
                if (ImGui::RadioButton(ss.str().c_str(), current_selection == i)) { 
                    cout << "selected " << i << endl;
                    current_selection = i; 
                }
            }
            
            if(ImGui::Button("OK")) {
                auto info = available_systems[current_selection];
                CreateSystem(info);
                CloseWindow();
            }
            //ImGui::PopTextWrapPos();
            ImGui::EndPopup();
        }
    }
    default:
        break;
    }
}

void ROMLoader::CreateSystem(System::Information const* info)
{
    shared_ptr<System> system = info->create_system();
    system->LoadROM(file_path_name);
    system_loaded->emit(shared_from_this(), system);
}

void ROMLoader::RegisterSystemInformation(System::Information const* info)
{
    ROMLoader::system_informations.push_back(info);
}

