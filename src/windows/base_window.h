#pragma once

#include "signals.h"

#include <memory>
#include <string>

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

private:
    std::string window_title;
    bool windowless;
    bool open;
    unsigned int window_id;

    static unsigned int next_id;
};