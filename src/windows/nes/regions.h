// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
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
    static char const * const GetWindowClassStatic() { return "Windows::NES::MemoryRegions"; }
    static std::shared_ptr<MemoryRegions> CreateWindow();

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::weak_ptr<System>            current_system;
    int selected_row;

};

} //namespace Windows::NES

