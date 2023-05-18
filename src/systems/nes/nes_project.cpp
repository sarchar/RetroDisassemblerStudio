#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>

#include "main.h"
#include "systems/nes/nes_cartridge.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"
#include "util.h"
#include "windows/nes/defines.h"
#include "windows/nes/labels.h"
#include "windows/nes/listing.h"
#include "windows/nes/regions.h"

using namespace std;

namespace NES {

BaseProject::Information const* Project::GetInformation()
{
    return Project::GetInformationStatic();
}

BaseProject::Information const* Project::GetInformationStatic()
{
    static Project::Information information = {
        .abbreviation   = "NES",
        .full_name      = "Nintendo Entertainment System",
        .is_rom_valid   = std::bind(&Project::IsROMValid, placeholders::_1, placeholders::_2),
        .create_project = std::bind(&Project::CreateProject),
    };

    return &information;
}

shared_ptr<BaseProject> Project::CreateProject()
{
    return make_shared<Project>();
}

Project::Project()
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

    create_new_project_progress->emit(shared_from_this(), false, 0, 0, "Loading file...");

    // Read in the iNES header
    ifstream rom_stream(file_path_name, ios::binary);
    if(!rom_stream) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Could not open file");
        return false;
    }

    unsigned char buf[16];
    rom_stream.read(reinterpret_cast<char*>(buf), 16);
    if(!rom_stream || !(buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Not an NES ROM file");
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
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the PRG rom data
        unsigned char data[16 * 1024];
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading PRG-ROM");
            return false;
        }

        // Get the PRG-ROM bank
        auto prg_bank = cartridge->GetProgramRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        prg_bank->InitializeFromData(reinterpret_cast<u8*>(data), sizeof(data)); // Initialize the memory region with bytes of data
    }

    // Load the CHR banks
    for(u32 i = 0; i < cartridge->header.num_chr_rom_banks; i++) {
        stringstream ss;
        ss << "Loading CHR ROM bank " << i;
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the CHR rom data
        unsigned char data[8 * 1024]; // TODO read 4K banks with other mappers (check chr_bank first!)
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading CHR-ROM");
            return false;
        }

        // Get the CHR bank
        auto chr_bank = cartridge->GetCharacterRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        chr_bank->InitializeFromData(reinterpret_cast<u8*>(data), sizeof(data)); // Mark content starting at offset 0 as data
    }

    // create labels for reset and the registers, etc
    system->CreateDefaultDefines();
    system->CreateDefaultLabels();

    create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, "Done");
    this_thread::sleep_for(std::chrono::seconds(1));

    cout << "[NES::Project] CreateNewProjectFromFile end" << endl;
    return true;
}

void Project::CreateDefaultWorkspace()
{
    auto app = MyApp::Instance();

    shared_ptr<BaseWindow> wnd = Windows::Labels::CreateWindow();
    wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    app->AddWindow(wnd);

    wnd = Windows::Defines::CreateWindow();
    wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    app->AddWindow(wnd);

    wnd = Windows::MemoryRegions::CreateWindow();
    wnd->SetInitialDock(BaseWindow::DOCK_LEFT);
    app->AddWindow(wnd);

    wnd = Listing::CreateWindow();
    wnd->SetInitialDock(BaseWindow::DOCK_ROOT);
    app->AddWindow(wnd);

}

bool Project::Save(std::ostream& os, std::string& errmsg) 
{
    // call the base method first to inject the project Information
    if(!BaseProject::Save(os, errmsg)) return false;

    // save the System structure
    auto system = GetSystem<System>();
    if(!system->Save(os, errmsg)) return false;

    return true;
}

bool Project::Load(std::istream& is, std::string& errmsg)
{
    if(!BaseProject::Load(is, errmsg)) return false;

    current_system = make_shared<System>();
    auto system = GetSystem<System>();
    if(!system->Load(is, errmsg)) return false;

    return true;
}

}
