#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "base_window.h"

using namespace std;

// not thread safe, all windows need to be created on the main thread
unsigned int BaseWindow::next_id = 0;

BaseWindow::BaseWindow(MyApp* app, string const& title)
    : main_application(app),
      hidden(false)
{
    stringstream ss;
    ss << title << "##" << BaseWindow::next_id++;
    window_title = ss.str();
}

BaseWindow::~BaseWindow()
{
}

void BaseWindow::Update(double deltaTime)
{
    UpdateContent(deltaTime);
}

void BaseWindow::RenderGUI()
{
    if(hidden) return;

    if(ImGui::Begin(window_title.c_str())) {
        RenderContent();
    }
    ImGui::End(); // always call end regardless of Begin()'s return value
}
