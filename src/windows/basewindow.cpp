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

    return string(buf);
}

BaseWindow::BaseWindow()
    : open(true), focused(false), docked(false), initial_dock_position(DOCK_NONE)
{
    // create the signals
    command_signal = make_shared<command_signal_t>();
    window_activated = make_shared<window_activated_t>();
    window_closed = make_shared<window_closed_t>();
    child_window_added = make_shared<child_window_added_t>();
    child_window_removed = make_shared<child_window_removed_t>();
    window_parented = make_shared<window_parented_t>();

    window_id = GetRandomID();

    SetTitle("<untitled>");
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
    ss << t << "###" << window_id;
    window_title = ss.str();

    stringstream idstr;
    idstr << window_title << "_DockSpace" << endl;
    dockspace_id = idstr.str();
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
    window->parent_window = shared_from_this();
    cout << WindowPrefix() << "Added child window \"" << window->window_title << "\" (managed window count = " << child_windows.size() << ")" << endl;
    *window->window_closed += std::bind(&BaseWindow::ChildWindowClosedHandler, this, placeholders::_1);
    queued_windows_for_add.push_back(window);
    window->window_parented->emit(shared_from_this());
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
    // Remove any windows queued for deletion
    ProcessQueuedChildWindowsForDelete();

    // Add any new windows
    ProcessQueuedChildWindowsForAdd();

    // post activated event 
    if(activated) window_activated->emit(shared_from_this());

    // only scan input if the window is receiving focus
    if(focused) CheckInput();

    // Render content of the window
    Update(deltaTime);

    // Update all child windows
    for(auto &window : child_windows) {
        window->InternalUpdate(deltaTime);
    }

}

void BaseWindow::InternalRender()
{
    InternalPreRender();

    // 'windowless' windows are essentially background tasks that have no GUI window associated with them
    // but a GUI callback is called regardless for popups and more
    if(windowless) {
        Render();

        // child windows still need to be rendered
        for(auto &window : child_windows) window->InternalRender();
    } else if(open) {
        // otherwise a window can be opened and its content rendered within
        bool local_open = open;
    
        // TODO cache these
        ImGuiWindowFlags window_flags = 0;
        if(is_mainwindow) window_flags |= ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse 
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
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);

            client_size = viewport->Size;
            // Adjust dockspace height for the status bar, if shown
            if(show_statusbar) client_size.y -= ImGui::GetFrameHeight();

            ImGui::SetNextWindowSize(client_size, ImGuiCond_Always);
            ImGui::SetNextWindowViewport(viewport->ID);
        } else {
            // TODO make this size configurable in BaseWindow
            ImGui::SetNextWindowSizeConstraints(ImVec2(250, 100),  ImVec2(1200, 800));
        }

        bool was_focused = focused;
        focused = false;
        docked = false;

        // for dockspaces, make sure we utilize the entire window area
        if(is_dockspace) {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        }

        auto visible = ImGui::Begin(window_title.c_str(), &local_open, window_flags);

        if(print_id) {
            auto imgui_window = ImGui::GetCurrentWindow();
            cout << WindowPrefix() << "window " << window_title << " has ID 0x" << hex << imgui_window->ID << endl;
            print_id = false;
        }

        // Dockspaces need to be kept alive if the window is hidden, otherwise docked windows will be undocked!
        InternalDockSpace(client_size.x, client_size.y);

        if(visible) {
            docked = ImGui::IsWindowDocked();

            focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) 
                && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

            Render();
            InternalRenderMenuBar();
            InternalRenderStatusBar();
        }

        activated = focused && !was_focused;

        // for dockspace children, return style var back to normal
        if(is_dockspace) ImGui::PopStyleVar(3);

        // render child windows inside Begin/End to keep the ID stack healthy,
        // but outside the success of ImGui::Begin() so that if the contents of that
        // window are hidden, child windows can still be visible, and some of them may
        // have their own dockspaces, which also need to be kept alive
        for(auto &window : child_windows) window->InternalRender();
    
        ImGui::End(); // always call end regardless of Begin()'s return value

        // close window if ImGui requested
        if(!local_open) CloseWindow();
    }

    InternalPostRender();
}

void BaseWindow::InternalPreRender()
{
    if(initial_dock_position != DOCK_NONE) {
        // Loop through parents looking for a dockspaces
        auto p = parent_window;
        while(p && !p->is_dockspace) p = p->parent_window;

        if(p && p->dockspace_is_built) {
            int dock_node_id = -1;

            switch(initial_dock_position) {
            case DOCK_LEFT:
                dock_node_id = p->imgui_dock_builder_left_id;
                break;

            case DOCK_RIGHT:
                dock_node_id = p->imgui_dock_builder_right_id;
                break;

            case DOCK_TOPRIGHT:
                dock_node_id = p->imgui_dock_builder_topright_id;
                break;

            case DOCK_BOTTOMRIGHT:
                dock_node_id = p->imgui_dock_builder_bottomright_id;
                break;

            case DOCK_BOTTOM:
                dock_node_id = p->imgui_dock_builder_bottom_id;
                break;

            case DOCK_ROOT:
                dock_node_id = p->imgui_dock_builder_root_id;
                break;

            default:
                break;
            }

            // Initialize this window on specified dock
            ImGui::SetNextWindowDockID(dock_node_id, ImGuiCond_Always);
            initial_dock_position = DOCK_NONE;
        }
    }

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

    imgui_dockspace_id = ImGui::GetID(dockspace_id.c_str());

    ImGui::DockSpace(imgui_dockspace_id, ImVec2(-1, -1), dockspace_flags);
    imgui_dock_builder_root_id = imgui_dockspace_id;

    // create the dockspace areas
    if(skip_dockspace_builder) dockspace_is_built = true;

    if(!dockspace_is_built) {
        // Create the root node, which we can use to dock windows
        imgui_dock_builder_root_id = ImGui::DockBuilderAddNode(imgui_dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);

        // initialize to the current window size
        ImGuiViewport* viewport = ImGui::GetWindowViewport();
        ImGui::DockBuilderSetNodeSize(imgui_dock_builder_root_id, viewport->Size);
        
        // split the dockspace into left and right, with the right side a temporary ID
        ImGuiID right_id;
        imgui_dock_builder_left_id = ImGui::DockBuilderSplitNode(imgui_dock_builder_root_id, ImGuiDir_Left, 0.3f, nullptr, &right_id);
        
        // split the right area, creating a temporary top/bottom
        ImGuiID top_id;
        imgui_dock_builder_bottom_id = ImGui::DockBuilderSplitNode(right_id, ImGuiDir_Down, 0.5f, nullptr, &top_id);
        
        // now split the top area into a middle and right
        imgui_dock_builder_right_id = ImGui::DockBuilderSplitNode(top_id, ImGuiDir_Right, 0.4f, nullptr, nullptr);

        // and split the right area into top and bottom
        imgui_dock_builder_topright_id = ImGui::DockBuilderSplitNode(imgui_dock_builder_right_id, ImGuiDir_Up, 0.5f, nullptr, &imgui_dock_builder_bottomright_id);

        ImGui::DockBuilderFinish(imgui_dockspace_id);
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

bool BaseWindow::SaveWorkspace(ostream& os, string& errmsg)
{
    if(no_save) return true;

    if(!InternalSaveWindow(os, errmsg)) return false;

    WriteVarInt(os, child_windows.size());
    if(!os.good()) { 
        errmsg = "Error saving Workspace " + WindowPrefix();
        return false;
    }

    for(auto& wnd : child_windows) {
        string window_class = wnd->GetWindowClass();
        WriteString(os, window_class);
        if(!os.good()) {
            errmsg = "Error saving window " + window_class;
            return true;
        }

        cout << WindowPrefix() << "saving child " << window_class << endl;
        if(!wnd->SaveWorkspace(os, errmsg)) return false;
    }

    return true;
}

bool BaseWindow::LoadWorkspace(istream& is, string& errmsg)
{
    if(no_save) return true;

    if(!InternalLoadWindow(is, errmsg)) return false;

    int num_child_windows = ReadVarInt<int>(is);
    if(!is.good()) {
        errmsg = "Error loading workspace";
        return false;
    }

    for(int i = 0; i < num_child_windows; i++) {
        string window_class;
        ReadString(is, window_class);

        cout << WindowPrefix() << "creating child " << window_class << endl;
        auto wnd = GetApplication()->CreateWindow(window_class);
        assert(wnd);
        
        // add child first, then load it in case any load mechanisms require parent_window
        AddChildWindow(wnd);

        if(!wnd->LoadWorkspace(is, errmsg)) return false;
    }

    return true;
}

bool BaseWindow::InternalSaveWindow(ostream& os, string& errmsg)
{
    // saving and restoring the window ID allows the dockspace to be saved in the ImGUI ini file
    WriteString(os, window_id);
    if(!os.good()) {
        errmsg = "Error in InternalSaveWindow";
        return false;
    }

    // save dockspace IDs since they're required to properly save window dock locations
    WriteVarInt(os, (int)is_dockspace);
    if(is_dockspace) {
        WriteVarInt(os, imgui_dock_builder_root_id);
        WriteVarInt(os, imgui_dock_builder_left_id);
        WriteVarInt(os, imgui_dock_builder_right_id);
        WriteVarInt(os, imgui_dock_builder_topright_id);
        WriteVarInt(os, imgui_dock_builder_bottomright_id);
        WriteVarInt(os, imgui_dock_builder_bottomright_id);
    }

    // finally save window specific content
    if(!SaveWindow(os, errmsg)) return false;
    return true;
}

bool BaseWindow::InternalLoadWindow(istream& is, string& errmsg)
{
    ReadString(is, window_id);
    if(!is.good()) {
        errmsg = "Error in InternalLoadWindow";
        return false;
    }

    // update window_title
    SetTitle(base_title);
    cout << WindowPrefix() << "changed ID to " << window_title << endl;

    // load dockspace IDs
    is_dockspace = (bool)ReadVarInt<int>(is);
    if(is_dockspace) {
        imgui_dock_builder_root_id = ReadVarInt<unsigned int>(is);
        imgui_dock_builder_left_id = ReadVarInt<unsigned int>(is);
        imgui_dock_builder_right_id = ReadVarInt<unsigned int>(is);
        imgui_dock_builder_topright_id = ReadVarInt<unsigned int>(is);
        imgui_dock_builder_bottomright_id = ReadVarInt<unsigned int>(is);
        imgui_dock_builder_bottomright_id = ReadVarInt<unsigned int>(is);
        dockspace_is_built = true;
    }

    if(!is.good()) {
        errmsg = "Error in InternalLoadWindow";
        return false;
    }

    // use the dock position from ImGUI INI
    initial_dock_position = DOCK_NONE;

    // finally save window specific content
    if(!LoadWindow(is, errmsg)) return false;
    return true;
}

} // namespace Windows

