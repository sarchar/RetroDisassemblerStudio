#pragma once

#include <memory>
#include <stack>
#include <thread>

#include "../main.h"
#include "signals.h"
#include "windows/basewindow.h"

#define GetMainWindow() GetApplication()->GetMainWindowAs<Windows::MainWindow>()
#define GetSystemInstance() (assert(GetMainWindow()); GetMainWindow()->GetMostRecentSystemInstance())

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
    bool OKPopup(std::string const& title, std::string const& content, 
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
            bool resizeable = false);

    // Project
    inline std::shared_ptr<BaseProject> GetCurrentProject() { return current_project; }

    // System Instance
    inline std::shared_ptr<BaseWindow> const& GetMostRecentSystemInstance() const { return most_recent_system_instance; }

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

    void ChildWindowAdded(std::shared_ptr<BaseWindow> const&);
    void ChildWindowRemoved(std::shared_ptr<BaseWindow> const&);

    void CreateNewProject(std::string const&);
    void ProjectCreatedHandler(std::shared_ptr<BaseWindow>, std::shared_ptr<BaseProject>);

    void CloseProject();

    void OpenROMInfosPane();

    // Popups
    bool StartPopup(std::string const&, bool);
    int  EndPopup(int, bool show_ok = true, bool show_cancel = true, bool allow_escape = true, bool focus_ok = false);
    void RenderPopups();

    void EditCommentPopup();
    void DisassemblyPopup();
    void SaveProjectPopup();
    void SaveProjectThread();
    void LoadProjectPopup();
    void LoadProjectThread();

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
    } popups;

    std::string current_popup_title;

    std::shared_ptr<BaseProject> current_project;
    std::string project_file_path;

    std::shared_ptr<BaseWindow> most_recent_system_instance;
};

}
