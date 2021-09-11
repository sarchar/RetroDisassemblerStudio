#pragma once

#include <iostream>
#include <string>
#include <functional>

class System {
public:
    struct Information {
        std::string abbreviation;
        std::string full_name;
        std::function<bool(std::string const&, std::istream&)> is_rom_valid;
        std::function<std::shared_ptr<System>()> create_system;
    };
    
public:
    System();
    virtual ~System();

    virtual System::Information const* GetInformation() = 0;
    virtual bool LoadROM(std::string const&) = 0;
};
