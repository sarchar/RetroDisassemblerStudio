#pragma once

#include "signals.h"

#include <memory>
#include <string>

class BaseWindow : public std::enable_shared_from_this<BaseWindow> {
public:
    BaseWindow(std::string const& title);
    virtual ~BaseWindow();

    // Utility
    void SetNav(bool _v) { enable_nav = _v; }
    void SetWindowless(bool _v) { windowless = _v; }
    bool IsWindowless() const { return windowless; }
    std::string const& GetTitle() const { return window_title; }
    void SetTitle(std::string const& t);
    std::string const& GetWindowID() const { return window_id; }
    void SetWindowID(std::string const& wid);

    void CloseWindow(); // emit window_closed and stop rendering

    bool IsFocused() const { return focused; }
    bool IsDocked() const { return docked; }

    // Called from the main application
    void Update(double deltaTime);
    void RenderGUI();

    typedef signal<std::function<void(std::shared_ptr<BaseWindow>)>> window_closed_t;
    std::shared_ptr<window_closed_t> window_closed;

    virtual char const * const GetWindowClass() = 0;

protected:
    // Implemented by derived class
    virtual void UpdateContent(double deltaTime) {};

    virtual void PreRenderContent() {};
    virtual void RenderContent() {};

    // Required in the derived class
    static std::string GetRandomID();

private:
    std::string window_title;
    std::string base_title;
    std::string window_tag; // window tag is used in ImGui window titles to keep the IDs unique
    std::string window_id;
    bool windowless;
    bool open;
    bool focused;
    bool docked;
    bool enable_nav;
};
