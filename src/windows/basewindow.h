// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
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
        DOCK_RIGHTTOP,
        DOCK_RIGHTBOTTOM,
        DOCK_BOTTOM,
        DOCK_BOTTOMLEFT,
        DOCK_BOTTOMRIGHT
    };

    BaseWindow();
    virtual ~BaseWindow();

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
    void SetIsDockSpace(bool _v, bool _skip_builder = false) { is_dockspace = _v; skip_dockspace_builder = _skip_builder; }
    void SetDockable(bool _v) { is_dockable = _v; }
    void SetMainWindow(bool _v) { is_mainwindow = _v; }
    void SetHideOnClose(bool _v) { hide_on_close = _v; }
    void SetHorizontalScroll(bool _v) { horizontal_scroll = _v; }

    template <class T>
    std::shared_ptr<T> As() { 
        return dynamic_pointer_cast<T>(shared_from_this());
    }

    std::string WindowPrefix() { return std::string("[") + GetWindowClass() + std::string("] "); }

    template<typename T>
    void IterateChildWindows(T const& func) {
        for(auto& wnd : child_windows) func(wnd);
    }

    void Show() { hidden = false; }

    void CloseWindow(); // emit window_closed and stop rendering

    bool IsFocused() const { return focused; }
    bool IsDocked() const { return docked; }
    bool WasActivated() const { return activated; }

    // Hidden windows do not get rendered or updated, but are not deleted from memory
    bool IsHidden() const { return hidden; }

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

    // Saving and loading
    bool SaveWorkspace(std::ostream&, std::string&);
    bool LoadWorkspace(std::istream&, std::string&);

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

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> window_parented_t;
    std::shared_ptr<window_parented_t> window_parented;

    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&)>> window_hidden_t;
    std::shared_ptr<window_hidden_t> window_hidden;

protected:
    // Implemented by derived class
    virtual void Update(double deltaTime) {};

    virtual void PreRender() {}; // good for i.e., push style vars that affect frames
    virtual void Render() {};
    virtual void PostRender() {}; // pop styles
    virtual void RenderMenuBar() {}
    virtual void RenderStatusBar() {}

    virtual void CheckInput() {};

    // override if no_save is not set!
    virtual bool SaveWindow(std::ostream&, std::string&) {
        std::cout << WindowPrefix() << "SaveWindow()" << std::endl;
        //assert(false);
        //return false;
        return true;
    }

    virtual bool LoadWindow(std::istream&, std::string&) {
        std::cout << WindowPrefix() << "LoadWindow()" << std::endl;
        //assert(false);
        //return false;
        return true;
    }

    // Required in the derived class
    static std::string GetRandomID();

private:
    std::string window_title;
    std::string base_title;
    std::string window_id;
    std::string dockspace_id;
    std::shared_ptr<BaseWindow> parent_window;
    InitialDockPosition initial_dock_position;

    bool open;
    bool focused;
    bool docked;
    bool activated;
    bool hidden;

    bool no_save = false;

    bool windowless = false;
    bool enable_nav = true;
    bool no_scrollbar = false;

    bool is_mainwindow = false;

    bool is_dockspace = false;
    bool skip_dockspace_builder = false;
    bool is_dockable = true;

    bool show_statusbar = false;
    bool show_menubar = false;

    bool hide_on_close = false;

    bool horizontal_scroll = false;

    bool dockspace_is_built = false;
    unsigned int imgui_dockspace_id;
    unsigned int imgui_dock_builder_root_id;
    unsigned int imgui_dock_builder_left_id;
    unsigned int imgui_dock_builder_right_id;
    unsigned int imgui_dock_builder_righttop_id;
    unsigned int imgui_dock_builder_rightbottom_id;
    unsigned int imgui_dock_builder_bottom_id;
    unsigned int imgui_dock_builder_bottomleft_id;
    unsigned int imgui_dock_builder_bottomright_id;

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

    bool InternalSaveWindow(std::ostream&, std::string&);
    bool InternalLoadWindow(std::istream&, std::string&);

    friend class Application;

    bool print_id = true;
};

} // namespace Windows
