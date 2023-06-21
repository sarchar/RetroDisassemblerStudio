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
    class System;
}

namespace Windows::NES {

class QuickExpressions : public BaseWindow {
public:
    using System = Systems::NES::System;

    QuickExpressions();
    virtual ~QuickExpressions();

    virtual char const * const GetWindowClass() { return QuickExpressions::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::QuickExpressions"; }
    static std::shared_ptr<QuickExpressions> CreateWindow();

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::shared_ptr<System> current_system;
    int selected_row;

    struct QuickExpressionData {
        std::string expression_string;
        s64         expression_value;
    };
    std::vector<QuickExpressionData> expressions;

    bool need_reiterate = true;
    bool need_resort    = true;
    int  sort_column    = 0;
    bool reverse_sort   = false;
    void Reiterate();
    void Resort();
};

} //namespace Windows::NES

