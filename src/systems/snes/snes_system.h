#pragma once

#include <string>
#include <iostream>
#include <memory>

#include "systems/system.h"

class SNESSystem : public System {
public:
    SNESSystem();
    virtual ~SNESSystem();
    
    System::Information const* GetInformation();
    bool LoadROM(std::string const&);

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<System> CreateSystem();
};
