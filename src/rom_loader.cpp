#include <iostream>
#include <memory>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "rom_loader.h"
#include "signals.h"
#include "systems/system.h"

using namespace std;

shared_ptr<ROMLoader> ROMLoader::CreateWindow(string const& _file_path_name)
{
    return make_shared<ROMLoader>(_file_path_name);
}

ROMLoader::ROMLoader(string const& _file_path_name)
    : BaseWindow("ROM Loader"),
      file_path_name(_file_path_name)
{
    SetHidden(true);
    system_loaded = make_shared<system_loaded_t>();
}

ROMLoader::~ROMLoader()
{
}

void ROMLoader::UpdateContent(double deltaTime) 
{
    static double timer = 0;
    timer += deltaTime;
    if(timer >= 3.0) {
        system_loaded->emit(shared_from_this(), nullptr);
        CloseWindow();
    }
}

void ROMLoader::RenderContent() 
{
    ImGui::Text("ROMLoader class");
}

//shared_ptr<System> ROMLoader::CreateNewSystem()
//{
//    // loop over all systems asking if the file name is valid
//    // if there's only 1 valid system, load it
//    // if there's more than 1, ask the user to clarify which system should load it
//    // returns a System instance
//}
