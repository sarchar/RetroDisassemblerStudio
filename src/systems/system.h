#pragma once

#include <iostream>
#include <string>
#include <functional>

#include "signals.h"
#include "util.h"

class BaseSystem : public std::enable_shared_from_this<BaseSystem> {
public:
    struct Information {
        std::string abbreviation;
        std::string full_name;
        std::function<bool(std::string const&, std::istream&)> is_rom_valid;
        std::function<std::shared_ptr<BaseSystem>()> create_system;
    };
    
public:
    BaseSystem();
    virtual ~BaseSystem();

    virtual BaseSystem::Information const* GetInformation() = 0;
    virtual bool CreateNewProjectFromFile(std::string const&) = 0;

    // common signals
    typedef signal<std::function<void(std::shared_ptr<BaseSystem>, bool error, u64 max_progress, u64 current_progress, std::string const& msg)>> create_new_project_progress_t;
    std::shared_ptr<create_new_project_progress_t> create_new_project_progress;
};
