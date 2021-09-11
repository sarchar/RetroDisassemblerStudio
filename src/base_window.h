#pragma once

#include "signals.h"

#include <memory>
#include <string>

class BaseWindow : public std::enable_shared_from_this<BaseWindow> {
public:
    BaseWindow(std::string const& title);
    virtual ~BaseWindow();

    // Utility
    void SetHidden(bool _v) { hidden = _v; }
    bool IsHidden() const { return hidden; }
    std::string const& GetTitle() const { return window_title; }

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
    bool hidden;
    bool open;

    static unsigned int next_id;
};
