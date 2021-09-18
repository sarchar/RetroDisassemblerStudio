#include <iostream>
#include <random>
#include <sstream>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"

#include "windows/base_window.h"

using namespace std;

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

BaseWindow::BaseWindow(string const& tag)
    : windowless(false), open(true), window_tag(tag)
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
    cout << GetWindowClass() << " title is " << ss.str() << endl;
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
