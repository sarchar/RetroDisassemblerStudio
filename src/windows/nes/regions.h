// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
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
    using MemoryRegion         = Systems::NES::MemoryRegion;
    using System               = Systems::NES::System;

    MemoryRegions(bool select_region, u32);
    virtual ~MemoryRegions();

    virtual char const * const GetWindowClass() { return MemoryRegions::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::MemoryRegions"; }
    static std::shared_ptr<MemoryRegions> CreateWindow();
    static std::shared_ptr<MemoryRegions> CreateWindow(bool select_region, u32 filter_address);

    // signals
    make_signal(region_selected, void(std::shared_ptr<MemoryRegion>));

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::weak_ptr<System>            current_system;
    int selected_row = -1;

    bool select_region;
    bool select_region_first_focus = true;
    std::string edit_buffer;
    u32  filter_address;
};

} //namespace Windows::NES

