#pragma once

#include "signals.h"

#include <memory>
#include <string>

#define BASE_WINDOW_FUNCTIONAL_CODE_DECL()          \
    protected:                                      \
        int& GetNextIDRef() { return next_id; }     \
    private:                                        \
        static int next_id 

#define BASE_WINDOW_FUNCTIONAL_CODE_IMPL(className) \
    int className::next_id = 0 

class BaseWindow : public std::enable_shared_from_this<BaseWindow> {
public:
    BaseWindow(std::string const& title);
    virtual ~BaseWindow();

    // Utility
    void SetWindowless(bool _v) { windowless = _v; }
    bool IsWindowless() const { return windowless; }
    std::string const& GetTitle() const { return window_title; }
    void SetTitle(std::string const& t);

    void CloseWindow(); // emit window_closed and stop rendering

    // Called from the main application
    void Update(double deltaTime);
    void RenderGUI();

    typedef signal<std::function<void(std::shared_ptr<BaseWindow>)>> window_closed_t;
    std::shared_ptr<window_closed_t> window_closed;

protected:
    // Implemented by derived class
    virtual void UpdateContent(double deltaTime) {};
    virtual void RenderContent() {};

    // Required in the derived class
    virtual int& GetNextIDRef() = 0;

private:
    std::string window_title;
    std::string window_tag; // window tag is used in ImGui window titles to keep the IDs unique
    bool windowless;
    bool open;
    int window_id;
};
