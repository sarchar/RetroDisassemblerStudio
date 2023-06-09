// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>
#include <stack>
#include <thread>

#include "imgui.h"

#include "main_application.h"
#include "signals.h"
#include "windows/basewindow.h"

#define GetMainWindow() GetApplication()->GetMainWindowAs<Windows::MainWindow>()

// Common flag buttons used in many windows
inline bool ImGuiFlagButton(bool* var, char const* text, char const* hover) {
    bool need_pop = false;
    bool pressed = false;
    if(var && *var) {
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(196, 0, 0));
        need_pop = true;
    }
    if(ImGui::SmallButton(text)) {
        if(var) *var = !(*var);
        pressed = true;
    }
    if(ImGui::IsItemHovered()) ImGui::SetTooltip(hover);
    if(need_pop) ImGui::PopStyleColor(2);
    return pressed;
}

namespace Windows {

class BaseProject;

// NES::Windows::System is home to everything you need about an instance of a NES system.  
// You can have multiple System windows, and that contains its own system state. 
// NES::System is generic and doesn't contain instance specific state. That information
// is designated to be here
class MainWindow : public BaseWindow {
public:
    MainWindow();
    virtual ~MainWindow();

    virtual char const * const GetWindowClass() { return MainWindow::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::MainWindow"; }
    static std::shared_ptr<MainWindow> CreateWindow();

    // Helper dialog boxes
    // Poups must be called every frame even if they're not open
    int  OKPopup(std::string const& title, std::string const& content, 
            bool resizeable = false);

    int  OKCancelPopup(std::string const& title, std::string const& content, 
            bool resizeable = false);

    int  InputNamePopup(std::string const& title, std::string const& label, std::string* buffer, 
            bool enter_returns_true = true, 
            bool resizeable = false);

    int  InputHexPopup(std::string const& title, std::string const& label, std::string* buffer, 
            bool enter_returns_true = true, 
            bool resizeable = false);

    int  InputMultilinePopup(std::string const& title, std::string const& label, std::string* buffer, 
            bool resizeable = false);

    // This is kind of a silly thing to do, but setting done to true means the dialog has been
    // showing and should now close (via CloseCurrentPopup)
    int  WaitPopup(std::string const& title, std::string const& content, bool done = false,
            bool cancelable = false,
            bool resizeable = false,
            bool wait_ok = false); // wait for OK to be pressed when done == true

    // Project
    inline std::shared_ptr<BaseProject> GetCurrentProject() { return current_project; }

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void PreRender() override;
    void Render() override;
    void PostRender() override;
    void RenderMenuBar() override;
    void RenderStatusBar() override;

private:
    bool show_imgui_demo;

    void UpdateApplicationTitle();
    void CreateNewProject(std::string const&);
    void ProjectCreatedHandler(std::shared_ptr<BaseWindow>, std::shared_ptr<BaseProject>);

    void CloseProject();

    void OpenROMInfosPane();

    // Popups
    bool StartPopup(std::string const&, bool, bool always_centered = false);
    int  EndPopup(int, bool show_ok = true, bool show_cancel = true, bool allow_escape = true, bool focus_ok = false);
    void RenderPopups();

    void EditCommentPopup();
    void DisassemblyPopup();
    void SaveProjectPopup();
    void SaveProjectThread();
    void LoadProjectPopup();
    void LoadProjectThread();
    void DeleteInstancePopup();

    // Global popups
    struct {
        struct {
            std::string title = "Saving Project...";
            std::shared_ptr<std::thread> thread;
            bool show = false;
            bool saving = false;
            bool errored = false;
            std::string errmsg;
        } save_project;

        struct {
            std::string title = "Loading Project...";
            std::shared_ptr<std::thread> thread;
            bool show = false;
            bool loading = true;
            bool errored = false;
            std::string errmsg;
        } load_project;

        struct {
            std::string title = "Delete Instance...";
            bool show = false;
            std::shared_ptr<BaseWindow> instance;
        } delete_instance;
    } popups;

    void StartSave();
    void StartSaveAs();

    std::string current_popup_title;

    std::shared_ptr<BaseProject> current_project;
    std::string project_file_path;
};

}
