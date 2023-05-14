#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "windows/base_window.h"

using namespace std;

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
    : windowless(false), open(true), focused(false), docked(false), enable_nav(true), window_tag(tag)
{
    // create the signals
    window_closed = make_shared<window_closed_t>();
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

    open = false;
    window_closed->emit(shared_from_this());
}

void BaseWindow::Update(double deltaTime)
{
    UpdateContent(deltaTime);
}

void BaseWindow::RenderGUI()
{
    PreRenderContent();

    // 'windowless' windows are essentially background tasks that have no GUI window associated with them
    // but a GUI callback is called regardless for popups and more
    if(windowless) {
        RenderContent();
        return;
    }

    // otherwise a window can be opened and its content rendered
    bool local_open = open;
    if(!local_open) return;

    // TODO make this size configurable in BaseWindow
    ImGui::SetNextWindowSizeConstraints(ImVec2(250, 100),  ImVec2(1200, 800));

    ImGuiWindowFlags window_flags = 0;
    if(!enable_nav) window_flags |= ImGuiWindowFlags_NoNav;

    bool is_open = ImGui::Begin(window_title.c_str(), &local_open, window_flags);
    focused = false;
    docked = ImGui::IsWindowDocked();

    if(is_open) {
        focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) 
            && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
        RenderContent();
    }

    ImGui::End(); // always call end regardless of Begin()'s return value

    if(!local_open) {
        CloseWindow();
    }
}
