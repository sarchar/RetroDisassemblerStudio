#pragma once

#include <memory>

#include "main.h"
#include "signals.h"
#include "systems/system.h"
#include "windows/base_window.h"

class SNESDebugger : public BaseWindow {
public:
    SNESDebugger();
    virtual ~SNESDebugger();

    // signals

private:
    // signal connections
    MyApp::current_system_changed_t::signal_connection_t current_system_changed_connection;

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void UpdateTitle();


public:
    static std::shared_ptr<SNESDebugger> CreateWindow();
};
