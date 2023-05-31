#pragma once

#include "signals.h"

#include <memory>
#include <string>

class Application;

#ifdef CreateWindow
#  undef CreateWindow
#endif

namespace Windows {

class BaseWindow : public std::enable_shared_from_this<BaseWindow> {
public:
    enum InitialDockPosition {
        DOCK_NONE, // don't dock when showing the window
        DOCK_ROOT,
        DOCK_LEFT,
        DOCK_RIGHT,
        DOCK_TOPRIGHT,
        DOCK_BOTTOMRIGHT,
        DOCK_BOTTOM
    };

    BaseWindow(std::string const& title);
    virtual ~BaseWindow();

    static void ResetWindowIDs();
    virtual char const * const GetWindowClass() = 0;

    // Utility
    void SetInitialDock(InitialDockPosition idp) { initial_dock_position = idp; } 
    void SetNav(bool _v) { enable_nav = _v; }
    void SetWindowless(bool _v) { windowless = _v; }
    void SetNoScrollbar(bool _v) { no_scrollbar = _v; }
    bool IsWindowless() const { return windowless; }
    std::string const& GetTitle() const { return window_title; }
    void SetTitle(std::string const& t);
    std::string const& GetWindowID() const { return window_id; }
    void SetWindowID(std::string const& wid);
    void SetShowStatusBar(bool enabled) { show_statusbar = false; /*enabled;*/ } // TODO statusbar no workie
    void SetShowMenuBar(bool enabled) { show_menubar = enabled; }
    void SetIsDockSpace(bool _v) { is_dockspace = _v; }
    void SetDockable(bool _v) { is_dockable = _v; }
    void SetMainWindow(bool _v) { is_mainwindow = _v; }

    template <class T>
    std::shared_ptr<T> As() { 
        return dynamic_pointer_cast<T>(shared_from_this());
    }

    std::string WindowPrefix() { return std::string("[") + GetWindowClass() + std::string("] "); }

    void CloseWindow(); // emit window_closed and stop rendering

    bool IsFocused() const { return focused; }
    bool IsDocked() const { return docked; }
    bool WasActivated() const { return activated; }

    template<class T>
    std::shared_ptr<T> GetParentWindowAs() {
        return dynamic_pointer_cast<T>(parent_window);
    }

    // Child Window support
    void AddChildWindow(std::shared_ptr<BaseWindow> const&);
    void CloseChildWindows();

    template <class T>
    std::shared_ptr<T> FindMostRecentChildWindow() {
        //TODO manage MRU stack
        for(auto &wnd : child_windows) {
            if(auto as_wnd = wnd->As<T>()) {
                return as_wnd;
            }
        }
        return nullptr;
    }

    // signals available in all windows
    typedef signal<std::function<void(std::shared_ptr<BaseWindow>&, std::string const&, void*)>> command_signal_t;
    std::shared_ptr<command_signal_t> command_signal;

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> window_activated_t;
    std::shared_ptr<window_activated_t> window_activated;

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> window_closed_t;
    std::shared_ptr<window_closed_t> window_closed;

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> child_window_added_t;
    std::shared_ptr<child_window_added_t> child_window_added;

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> child_window_removed_t;
    std::shared_ptr<child_window_removed_t> child_window_removed;

protected:
    // Implemented by derived class
    virtual void Update(double deltaTime) {};

    virtual void PreRender() {}; // good for i.e., push style vars that affect frames
    virtual void Render() {};
    virtual void PostRender() {}; // pop styles
    virtual void RenderMenuBar() {}
    virtual void RenderStatusBar() {}

    virtual void CheckInput() {};

    // Required in the derived class
    static std::string GetRandomID();

private:
    std::string window_title;
    std::string base_title;
    std::string window_tag; // window tag is used in ImGui window titles to keep the IDs unique
    std::string window_id;
    std::string dockspace_id;
    std::shared_ptr<BaseWindow> parent_window;
    InitialDockPosition initial_dock_position;

    bool open;
    bool focused;
    bool docked;
    bool activated;

    bool windowless = false;
    bool enable_nav = true;
    bool no_scrollbar = false;

    bool is_mainwindow = false;

    bool is_dockspace = false;
    bool is_dockable = true;

    bool show_statusbar = false;
    bool show_menubar = false;

    bool dockspace_is_built = false;
    unsigned int imgui_dockspace_id;
    unsigned int imgui_dock_builder_root_id;
    unsigned int imgui_dock_builder_left_id;
    unsigned int imgui_dock_builder_right_id;
    unsigned int imgui_dock_builder_topright_id;
    unsigned int imgui_dock_builder_bottomright_id;
    unsigned int imgui_dock_builder_bottom_id;

    // Managed child windows
    void ProcessQueuedChildWindowsForAdd();
    void ProcessQueuedChildWindowsForDelete();
    void ChildWindowClosedHandler(std::shared_ptr<BaseWindow> const&);

    std::vector<std::shared_ptr<BaseWindow>> child_windows;
    std::vector<std::shared_ptr<BaseWindow>> queued_windows_for_add;
    std::vector<std::shared_ptr<BaseWindow>> queued_windows_for_delete;

    // Called from the main application and parent windows
    void InternalUpdate(double deltaTime);
    void InternalPreRender();
    void InternalRender();
    void InternalPostRender();

    void InternalDockSpace(float, float);
    void InternalRenderMenuBar();
    void InternalRenderStatusBar();

    friend class Application;
};

} // namespace Windows
