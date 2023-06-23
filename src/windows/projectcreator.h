// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include "signals.h"
#include "systems/system.h"
#include "windows/basewindow.h"

#include <memory>
#include <string>
#include <vector>

namespace Windows {

class ProjectCreatorWindow : public BaseWindow {
public:
    ProjectCreatorWindow(std::string const& _file_path_name);
    virtual ~ProjectCreatorWindow();

    virtual char const * const GetWindowClass() { return ProjectCreatorWindow::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::ProjectCreatorWindow"; }
    static std::shared_ptr<ProjectCreatorWindow> CreateWindow();
    static std::shared_ptr<ProjectCreatorWindow> CreateWindow(std::string const& _file_path_name);

    // signals
    make_signal(project_created, void(std::shared_ptr<BaseWindow>, std::shared_ptr<BaseProject>));

protected:
    void Update(double deltaTime) override;
    void Render() override;

    void CreateProjectThreadMain();

private:
    std::string file_path_name;
    std::vector<BaseProject::Information const*> available_systems;

    enum {
        LOADER_STATE_INIT = 0,
        LOADER_STATE_FILE_NOT_FOUND,
        LOADER_STATE_NOT_A_VALID_ROM,
        LOADER_STATE_SELECT_SYSTEM,
        LOADER_STATE_CREATING_PROJECT
    } loader_state;

    void CreateNewProject(BaseProject::Information const*);
    void CreateNewProjectProgress(std::shared_ptr<BaseProject> system, bool error, u64 max_progress, u64 current_progress, std::string const& msg);

    std::unique_ptr<std::thread> create_project_thread;
    std::shared_ptr<BaseProject> current_project;
    u64         create_project_max_progress;
    u64         create_project_current_progress;
    std::string create_project_message;
    bool        create_project_error;
    bool        create_project_done;


};

} // namespace Windows
