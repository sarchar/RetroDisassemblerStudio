#pragma once

#include "base_window.h"
#include "signals.h"
#include "systems/system.h"

#include <memory>
#include <string>
#include <vector>

class ProjectCreatorWindow : public BaseWindow {
public:
    ProjectCreatorWindow(std::string const& _file_path_name);
    virtual ~ProjectCreatorWindow();

    virtual char const * const GetWindowClass() { return ProjectCreatorWindow::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "ProjectCreatorWindow"; }

    // signals
    typedef signal<std::function<void(std::shared_ptr<BaseWindow>, std::shared_ptr<BaseSystem>)>> system_loaded_t;
    std::shared_ptr<system_loaded_t> system_loaded;

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

    void CreateProjectThreadMain();

private:
    std::string file_path_name;
    std::vector<BaseSystem::Information const*> available_systems;

    enum {
        LOADER_STATE_INIT = 0,
        LOADER_STATE_FILE_NOT_FOUND,
        LOADER_STATE_NOT_A_VALID_ROM,
        LOADER_STATE_SELECT_SYSTEM,
        LOADER_STATE_CREATING_PROJECT
    } loader_state;

    void CreateNewProject(BaseSystem::Information const*);
    void CreateNewProjectProgress(std::shared_ptr<BaseSystem> system, bool error, u64 max_progress, u64 current_progress, std::string const& msg);

    std::unique_ptr<std::thread> create_project_thread;
    std::shared_ptr<BaseSystem> current_system;
    u64         create_project_max_progress;
    u64         create_project_current_progress;
    std::string create_project_message;
    bool        create_project_error;
    bool        create_project_done;

public:
    static void RegisterSystemInformation(BaseSystem::Information const*);
    static std::shared_ptr<ProjectCreatorWindow> CreateWindow(std::string const& _file_path_name);

private:
    static std::vector<BaseSystem::Information const*> system_informations;
};
