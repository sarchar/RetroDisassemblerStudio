#include <iostream>
#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "windows/base_window.h"

using namespace std;

BaseWindow::BaseWindow(string const& tag)
    : windowless(false), open(true), window_tag(tag)
{
    // start window_id at -1 for later
    window_id = -1;

    // create the signals
    window_closed = make_shared<window_closed_t>();
}

BaseWindow::~BaseWindow()
{
}

void BaseWindow::SetTitle(std::string const& t)
{
    stringstream ss;
    if(window_id < 0) window_id = GetNextIDRef()++;
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
    // 'windowless' windows are essentially background tasks that have no GUI window associated with them
    // but a GUI callback is called regardless for popups and more
    if(windowless) {
        RenderContent();
        return;
    }

    // otherwise a window can be opened and its content rendered
    bool local_open = open;
    if(!local_open) return;

    if(ImGui::Begin(window_title.c_str(), &local_open)) {
        RenderContent();
    }

    ImGui::End(); // always call end regardless of Begin()'s return value

    if(!local_open) {
        CloseWindow();
    }
}
