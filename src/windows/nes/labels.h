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
    class Label;
    class System;
}

namespace Windows::NES {

class Labels : public BaseWindow {
public:
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using Label                = Systems::NES::Label;
    using System               = Systems::NES::System;

    Labels();
    virtual ~Labels();

    virtual char const * const GetWindowClass() { return Labels::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Labels"; }
    static std::shared_ptr<Labels> CreateWindow();

    // signals

protected:
    void CheckInput() override;
    void Update(double deltaTime) override;
    void Render() override;

private:
    void LabelCreated(std::shared_ptr<Label> const&, bool);
    void LabelDeleted(std::shared_ptr<Label> const&, int);

    std::weak_ptr<System>            current_system;
    int selected_row;
    int context_row;

    std::vector<std::weak_ptr<Label>> labels;
    bool force_reiterate;
    bool force_resort;

    bool case_sensitive_sort;
    bool show_locals;

    signal_connection label_created_connection;
    signal_connection label_deleted_connection;
};

} //namespace Windows::NES

