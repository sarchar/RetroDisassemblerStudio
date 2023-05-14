#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "signals.h"
#include "util.h"

class BaseSystem;

class BaseProject : public std::enable_shared_from_this<BaseProject> {
public:
    struct Information {
        std::string abbreviation;
        std::string full_name;
        std::function<bool(std::string const&, std::istream&)> is_rom_valid;
        std::function<std::shared_ptr<BaseProject>()> create_project;
    };
    virtual Information const* GetInformation() = 0;

    BaseProject();
    virtual ~BaseProject();

    std::string                 GetRomFileName() const { return rom_file_name; }
    std::shared_ptr<BaseSystem> GetBaseSystem() { return current_system; }

    template <class T>
    std::shared_ptr<T> GetSystem() {
        return dynamic_pointer_cast<T>(GetBaseSystem());
    }

    // slow, call from separate thread
    virtual bool CreateNewProjectFromFile(std::string const&) = 0;

    virtual void CreateDefaultWorkspace() = 0;

    virtual bool Save(std::ostream&, std::string&);
    virtual bool Load(std::istream&, std::string&);
    static std::shared_ptr<BaseProject> LoadProject(std::istream&, std::string&);

    // signals
    typedef signal<std::function<void(std::shared_ptr<BaseProject>, bool error, 
            u64 max_progress, u64 current_progress, std::string const& msg)>> create_new_project_progress_t;
    std::shared_ptr<create_new_project_progress_t> create_new_project_progress;

protected:
    std::shared_ptr<BaseSystem> current_system;
    std::string                 rom_file_name;

public:
    static void RegisterProjectInformation(Information const*);
    static Information const* GetProjectInformation(int);
    static Information const* GetProjectInformation(std::string const& abbreviation);
    static std::vector<Information const*> project_informations;
};
