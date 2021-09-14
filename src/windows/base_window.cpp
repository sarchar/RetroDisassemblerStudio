#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "windows/base_window.h"

using namespace std;

// not thread safe, all windows need to be created on the main thread
unsigned int BaseWindow::next_id = 0;

BaseWindow::BaseWindow(string const& title)
    : windowless(false), open(true)
{
    // give the window an id
    window_id = BaseWindow::next_id++;

    // set the window title
    SetTitle(title);

    // create the signals
    window_closed = make_shared<window_closed_t>();
}

BaseWindow::~BaseWindow()
{
}

void BaseWindow::SetTitle(std::string const& t)
{
    stringstream ss;
    ss << t << "##" << BaseWindow::next_id++;
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
