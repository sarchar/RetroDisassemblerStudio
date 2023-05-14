#pragma once

#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class GlobalMemoryLocation;

namespace Windows {

class MemoryRegions : public BaseWindow {
public:
    MemoryRegions();
    virtual ~MemoryRegions();

    virtual char const * const GetWindowClass() { return MemoryRegions::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::MemoryRegions"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    std::weak_ptr<System>            current_system;
    int selected_row;

public:
    static std::shared_ptr<MemoryRegions> CreateWindow();
};

} //namespace Windows

} //namespace NES
