#include <algorithm>
#include <cassert>
#include <string>

#include "systems/snes/snes_system.h"

using namespace std;

SNESSystem::SNESSystem()
{
}

SNESSystem::~SNESSystem()
{
}

bool SNESSystem::LoadROM(string const& file_path_name)
{
    return false;
}

bool SNESSystem::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    // TODO this is duplicated, make it a utility function somewhere
    auto ends_with = [](std::string const& value, std::string const& ending) {
        if (ending.size() > value.size()) return false;
        return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
    };

    string lcase_file_path_name = file_path_name;
    std::transform(lcase_file_path_name.begin(), lcase_file_path_name.end(), 
                   lcase_file_path_name.begin(), [](unsigned char c){ return std::tolower(c); });

    if(ends_with(lcase_file_path_name, ".bin")) {
        return true;
    } else if(ends_with(lcase_file_path_name, ".smc")) {
        assert(false); // TODO
    }

    return false;
}

System::Information const* SNESSystem::GetInformation()
{
    return SNESSystem::GetInformationStatic();
}

System::Information const* SNESSystem::GetInformationStatic()
{
    static System::Information information = {
        .abbreviation = "SNES",
        .full_name = "Super Nintendo Entertainment System",
        .is_rom_valid = std::bind(&SNESSystem::IsROMValid, placeholders::_1, placeholders::_2),
        .create_system = std::bind(&SNESSystem::CreateSystem)
    };
    return &information;
}

shared_ptr<System> SNESSystem::CreateSystem()
{
    SNESSystem* snes_system = new SNESSystem();
    return shared_ptr<System>(snes_system);
}

