#pragma once

#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "windows/basewindow.h"

namespace Systems::NES {
    class GlobalMemoryLocation;
    class System;
}

namespace Windows::NES {

class MemoryRegions : public BaseWindow {
public:
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using System               = Systems::NES::System;

    MemoryRegions();
    virtual ~MemoryRegions();

    virtual char const * const GetWindowClass() { return MemoryRegions::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::MemoryRegions"; }

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::weak_ptr<System>            current_system;
    int selected_row;

public:
    static std::shared_ptr<MemoryRegions> CreateWindow();
};

} //namespace Windows::NES

