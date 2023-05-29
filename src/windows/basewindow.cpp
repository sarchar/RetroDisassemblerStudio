#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "windows/basewindow.h"

using namespace std;

namespace Windows {

static u64 base_window_next_id = 0;

void BaseWindow::ResetWindowIDs()
{
    // temporary code
    base_window_next_id = 0;
}

string BaseWindow::GetRandomID()
{
    static char const CHARSET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-+?=";
    static int CHARSET_SIZE = sizeof(CHARSET) / sizeof(CHARSET[0]);
    static random_device rd;
    static mt19937_64 gen(rd());

    char buf[12];
    for(int i = 0; i < 11; i++) {
        buf[i] = CHARSET[gen() % CHARSET_SIZE];
    }
    buf[11] = 0;

    //return string(buf);
    stringstream ss;
    ss << base_window_next_id++;
    return ss.str();
}

BaseWindow::BaseWindow(string const& tag)
    : windowless(false), open(true), focused(false), docked(false), enable_nav(true), 
      show_menubar(false), show_statusbar(false),
      is_mainwindow(false), is_dockspace(false), is_dockable(true),
      window_tag(tag), initial_dock_position(DOCK_NONE)
{
    // create the signals
    command_signal = make_shared<command_signal_t>();
    window_closed = make_shared<window_closed_t>();
    child_window_added = make_shared<child_window_added_t>();
    child_window_removed = make_shared<child_window_removed_t>();
}

BaseWindow::~BaseWindow()
{
}

void BaseWindow::SetWindowID(std::string const& wid)
{
    window_id = wid;
    SetTitle(base_title);
}

void BaseWindow::SetTitle(std::string const& t)
{
    base_title = t;

    stringstream ss;
    if(window_id.length() == 0) {
        window_id = BaseWindow::GetRandomID();
    }
    ss << t << "###" << window_tag << "_" << window_id;
    window_title = ss.str();
}

void BaseWindow::CloseWindow()
{
    if(!open) return;

    // close all child windows when we are closed
    CloseChildWindows();

    open = false;
    window_closed->emit(shared_from_this());
}

void BaseWindow::CloseChildWindows()
{
    for(auto& wnd : child_windows) wnd->CloseWindow();
}

void BaseWindow::AddChildWindow(shared_ptr<BaseWindow> const& window)
{
    cout << WindowPrefix() << "Added child window \"" << window->GetTitle() << "\" (managed window count = " << child_windows.size() << ")" << endl;
    *window->window_closed += std::bind(&BaseWindow::ChildWindowClosedHandler, this, placeholders::_1);
    queued_windows_for_add.push_back(window);
}

void BaseWindow::ChildWindowClosedHandler(std::shared_ptr<BaseWindow> const& window)
{
    cout << WindowPrefix() << "\"" << window->GetTitle() << "\" closed (managed window count = " << child_windows.size() + queued_windows_for_delete.size() - 1 << ")" << endl;
    queued_windows_for_delete.push_back(window);
}

void BaseWindow::ProcessQueuedChildWindowsForAdd()
{
    for(auto& window : queued_windows_for_add) {
        child_windows.push_back(window);
        child_window_added->emit(window);
    }

    queued_windows_for_add.resize(0);
}

void BaseWindow::ProcessQueuedChildWindowsForDelete()
{
    for(auto& window : queued_windows_for_delete) {
        auto it = find(child_windows.begin(), child_windows.end(), window);
        if(it != child_windows.end()) child_windows.erase(it);
        child_window_removed->emit(window);
    }

    queued_windows_for_delete.resize(0);
}

void BaseWindow::InternalUpdate(double deltaTime)
{
    // only scan input if the window is receiving focus
    if(focused) CheckInput();

    // Render content of the window
    Update(deltaTime);

    // Add any new windows
    ProcessQueuedChildWindowsForAdd();

    // Update all child windows
    for(auto &window : child_windows) {
        window->InternalUpdate(deltaTime);
    }

    // Remove any windows queued for deletion
    ProcessQueuedChildWindowsForDelete();
}

void BaseWindow::InternalRender()
{
    InternalPreRender();

    // 'windowless' windows are essentially background tasks that have no GUI window associated with them
    // but a GUI callback is called regardless for popups and more
    if(windowless) {
        Render();
    } else if(open) {
        // otherwise a window can be opened and its content rendered within
        bool local_open = open;
    
   
        // TODO cache these
        ImGuiWindowFlags window_flags = 0;
        if(is_mainwindow) window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse 
                                            | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                                            | ImGuiWindowFlags_NoBringToFrontOnFocus;
        if(!enable_nav || is_mainwindow) {
            window_flags |= ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoNavInputs;
        }
        if(no_scrollbar)  window_flags |= ImGuiWindowFlags_NoScrollbar;
        if(show_menubar)  window_flags |= ImGuiWindowFlags_MenuBar;
        if(!is_dockable)  window_flags |= ImGuiWindowFlags_NoDocking;

        // Adjust the window size if necessary
        ImVec2 client_size = ImGui::GetWindowSize();
        if(is_mainwindow) {
            //auto io = ImGui::GetIO();

            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            //ImGui::SetNextWindowPos(ImVec2(0, 0));

            client_size = viewport->Size;//io.DisplaySize;
            // Adjust dockspace height for the status bar, if shown
            if(show_statusbar) client_size.y -= ImGui::GetFrameHeight();

            ImGui::SetNextWindowSize(client_size, ImGuiCond_Appearing);
            ImGui::SetNextWindowViewport(viewport->ID);
        } else {
            // TODO make this size configurable in BaseWindow
            ImGui::SetNextWindowSizeConstraints(ImVec2(250, 100),  ImVec2(1200, 800));
        }

        focused = false;
        docked = false;

        if(ImGui::Begin(window_title.c_str(), &local_open, window_flags)) {
            docked = ImGui::IsWindowDocked();

            focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) 
                && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

            InternalDockSpace(client_size.x, client_size.y);
            Render();
            InternalRenderMenuBar();
            InternalRenderStatusBar();
        }
    
        ImGui::End(); // always call end regardless of Begin()'s return value

        // close window if ImGui requested
        if(!local_open) CloseWindow();
    }

    // Render child windows (they won't show if CloseWindow() was called
    for(auto &window : child_windows) window->InternalRender();

    InternalPostRender();
}


void BaseWindow::InternalPreRender()
{
//!    if(initial_dock_position != DOCK_NONE) {
//!        auto app = MyApp::Instance();
//!        if(app->HasDockBuilder()) {
//!            int dock_node_id = -1;
//!
//!            switch(initial_dock_position) {
//!            case DOCK_LEFT:
//!                dock_node_id = app->GetDockBuilderLeftID();
//!                break;
//!
//!            case DOCK_RIGHT:
//!                dock_node_id = app->GetDockBuilderRightID();
//!                break;
//!
//!            case DOCK_BOTTOM:
//!                dock_node_id = app->GetDockBuilderBottomID();
//!                break;
//!
//!            case DOCK_ROOT:
//!                dock_node_id = app->GetDockBuilderRootID();
//!                break;
//!
//!            default:
//!                break;
//!            }
//!
//!            // Initialize this window on specified dock
//!            ImGui::SetNextWindowDockID(dock_node_id, ImGuiCond_Appearing);
//!        }
//!    }

    PreRender();
}

void BaseWindow::InternalPostRender()
{
    PostRender();
}

// code from https://gist.github.com/PossiblyAShrub/0aea9511b84c34e191eaa90dd7225969
void BaseWindow::InternalDockSpace(float w, float h)
{
    if(!is_dockspace) return;

    ImGuiDockNodeFlags dockspace_flags = 0;

    // return if not supported
    ImGuiIO& io = ImGui::GetIO();
    if(!(io.ConfigFlags & ImGuiConfigFlags_DockingEnable)) return;

    auto imgui_dockspace_id = ImGui::GetID("##DockSpace");
    ImGui::DockSpace(imgui_dockspace_id, ImVec2(w, h), dockspace_flags);

    //TODO dock builder
    if(!dockspace_is_built) {
        ImGuiViewport* viewport = ImGui::GetWindowViewport();

        // Create the root node, which we can use to dock windows
        /*imgui_dock_builder_root_id =*/ ImGui::DockBuilderAddNode(imgui_dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        
        // And make it take the entire viewport
        ImGui::DockBuilderSetNodeSize(imgui_dockspace_id, viewport->Size);

        ImGui::DockBuilderFinish(imgui_dockspace_id);

        // do this last for race conditions
        dockspace_is_built = true;
    }
}

void BaseWindow::InternalRenderMenuBar()
{
    if(!show_menubar) return;

    if(is_mainwindow) {
        if(!ImGui::BeginMainMenuBar()) return;
    } else {
        if(!ImGui::BeginMenuBar()) return;
    }

    RenderMenuBar();

    if(is_mainwindow) ImGui::EndMainMenuBar();
    else ImGui::EndMenuBar();
}

// Render a tool/status bar in the window
// see https://github.com/ocornut/imgui/issues/3518#issuecomment-807398290
void BaseWindow::InternalRenderStatusBar()
{
    if(!show_statusbar) return;

    ImGuiViewport* viewport = (ImGuiViewportP*)(void*)ImGui::GetWindowViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - ImGui::GetFrameHeight()));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, viewport->Size.y - ImGui::GetFrameHeight()));

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs 
                                    | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar
                                    | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus
                                    | ImGuiWindowFlags_MenuBar
                                    | ImGuiWindowFlags_NoDocking;

    if(ImGui::Begin("##StatusBar", nullptr, window_flags)) {
        if(ImGui::BeginMenuBar()) {
            RenderStatusBar();
            ImGui::EndMenuBar();
        }
        ImGui::End();
    }
}

} // namespace Windows

