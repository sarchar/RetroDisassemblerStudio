#pragma once

#include "systems/system.h"

namespace NES {

enum MIRRORING {
    MIRRORING_HORIZONTAL = 0,
    MIRRORING_VERTICAL,
    MIRRORING_FOUR_SCREEN,
};

class Cartridge;

}

class NESSystem : public System {
public:
    NESSystem();
    virtual ~NESSystem();

    System::Information const* GetInformation();
    bool CreateNewProjectFromFile(std::string const&);

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<System> CreateSystem();

    std::shared_ptr<NES::Cartridge>& GetCartridge() { return cartridge; }
    std::shared_ptr<NES::Cartridge> cartridge;

private:
    std::string rom_file_path_name;
};
