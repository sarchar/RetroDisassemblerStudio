#pragma once

#include <memory>

#include "signals.h"
#include "systems/system.h"
#include "windows/base_window.h"

class SNESDebugger : public BaseWindow {
public:
    SNESDebugger();
    virtual ~SNESDebugger();

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void UpdateTitle();

public:
    static std::shared_ptr<SNESDebugger> CreateWindow();
};
