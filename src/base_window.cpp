#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "base_window.h"

using namespace std;

// not thread safe, all windows need to be created on the main thread
unsigned int BaseWindow::next_id = 0;

BaseWindow::BaseWindow(string const& title)
    : hidden(false), open(true)
{
    stringstream ss;
    ss << title << "##" << BaseWindow::next_id++;
    window_title = ss.str();

    // create the signals
    window_closed = make_shared<window_closed_t>();
}

BaseWindow::~BaseWindow()
{
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
    bool local_open = open;

    // 'hidden' windows are essentially background tasks that have no GUI window associated with them
    if(hidden) return;

    if(!local_open) return;

    if(ImGui::Begin(window_title.c_str(), &local_open)) {
        RenderContent();
    }

    ImGui::End(); // always call end regardless of Begin()'s return value

    if(!local_open) {
        CloseWindow();
    }
}
