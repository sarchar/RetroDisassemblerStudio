#pragma once

#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class GlobalMemoryLocation;
class Label;

namespace Windows {

class Labels : public BaseWindow {
public:
    Labels();
    virtual ~Labels();

    virtual char const * const GetWindowClass() { return Labels::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Labels"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void LabelCreated(std::shared_ptr<Label> const&, bool);

    std::weak_ptr<System>            current_system;
    int selected_row;

    std::vector<std::weak_ptr<Label>> labels;
    bool force_reiterate;
    bool force_resort;

    bool case_sensitive_sort;
    bool show_locals;

    System::label_created_t::signal_connection_t label_created_connection;

public:
    static std::shared_ptr<Labels> CreateWindow();
};

} //namespace Windows

} //namespace NES