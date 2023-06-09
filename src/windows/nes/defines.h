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

namespace Systems {
namespace NES {
class System;
class Define;
}
}

namespace Windows {
namespace NES {

class Defines : public BaseWindow {
public:
    using Define = Systems::NES::Define;
    using System = Systems::NES::System;

    Defines();
    virtual ~Defines();

    virtual char const * const GetWindowClass() { return Defines::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::Defines"; }
    static std::shared_ptr<Defines> CreateWindow();

    void Highlight(std::shared_ptr<Systems::NES::Define>&);

protected:
    void Update(double deltaTime) override;
    void Render() override;
    void CheckInput() override;

private:
    void RenderToolBar();
    void DefineCreated(std::shared_ptr<Systems::NES::Define> const&);

    std::weak_ptr<Systems::NES::System> current_system;
    int selected_row = -1;
    int context_row = 0;

    // defines
    std::vector<std::weak_ptr<Systems::NES::Define>> defines;
    void Resort();
    bool need_reiterate = true;
    bool need_resort = true;
    bool case_sensitive_sort = false;
    int  sort_column = -1;
    bool reverse_sort = false;

    // creating and editing new defines
    void RenderCreateNewDefineRow();
    void RenderExpressionColumn(std::shared_ptr<Define> const&);
    bool creating_new_define = false;
    bool editing_expression = false;
    bool started_editing = false;
    std::string edit_buffer;
    bool wait_dialog = false;
    std::string wait_dialog_message;
    std::shared_ptr<Systems::NES::Define> edit_define;

    // deleting
    void DeleteDefine(int);

    std::shared_ptr<Systems::NES::Define> highlight;

    signal_connection define_created_connection;
};

} //namespace NES

} //namespace Windows
