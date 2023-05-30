#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include "main.h"
#include "signals.h"
#include "util.h"

#include "windows/basewindow.h"

class BaseSystem;

namespace Windows {

class BaseProject : public BaseWindow {
public:
    struct Information {
        std::string abbreviation;
        std::string full_name;
        std::function<bool(std::string const&, std::istream&)> is_rom_valid;
        std::function<std::shared_ptr<BaseProject>()> create_project;
    };
    virtual Information const* GetInformation() = 0;

    BaseProject(std::string const&);
    virtual ~BaseProject();

    std::string                 GetRomFileName() const { return rom_file_name; }
    std::shared_ptr<BaseSystem> GetBaseSystem() { return current_system; }

    template <class T>
    std::shared_ptr<T> GetSystem() {
        return dynamic_pointer_cast<T>(GetBaseSystem());
    }

    // slow, call from separate thread
    virtual bool CreateNewProjectFromFile(std::string const&) = 0;

    virtual void CreateSystemInstance() = 0;

    virtual bool Save(std::ostream&, std::string&);
    virtual bool Load(std::istream&, std::string&);
    static std::shared_ptr<BaseProject> StartLoadProject(std::istream&, std::string&);

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

private:
    virtual void WindowAdded(std::shared_ptr<BaseWindow> const&) {}
    virtual void WindowRemoved(std::shared_ptr<BaseWindow> const&) {}
    void _WindowAdded(std::shared_ptr<BaseWindow> const& window) { WindowAdded(window); }
    void _WindowRemoved(std::shared_ptr<BaseWindow> const& window) { WindowRemoved(window); }
    signal_connection window_added_connection;
    signal_connection window_removed_connection;
};

}
