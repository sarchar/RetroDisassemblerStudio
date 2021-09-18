#pragma once

#include "base_window.h"
#include "signals.h"
#include "systems/system.h"

#include <memory>
#include <string>
#include <vector>

class ROMLoader : public BaseWindow {
public:
    ROMLoader(std::string const& _file_path_name);
    virtual ~ROMLoader();

    virtual char const * const GetWindowClass() { return ROMLoader::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "ROMLoader"; }

    // signals
    typedef signal<std::function<void(std::shared_ptr<BaseWindow>, std::shared_ptr<System>)>> system_loaded_t;
    std::shared_ptr<system_loaded_t> system_loaded;

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    std::string file_path_name;
    std::vector<System::Information const*> available_systems;

    enum {
        LOADER_STATE_INIT = 0,
        LOADER_STATE_FILE_NOT_FOUND,
        LOADER_STATE_NOT_A_VALID_ROM,
        LOADER_STATE_SELECT_SYSTEM
    } loader_state;

    void CreateSystem(System::Information const*);

public:
    static void RegisterSystemInformation(System::Information const*);
    static std::shared_ptr<ROMLoader> CreateWindow(std::string const& _file_path_name);

private:
    static std::vector<System::Information const*> system_informations;
};
