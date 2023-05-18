#pragma once

#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class Define;

namespace Windows {

class Defines : public BaseWindow {
public:
    Defines();
    virtual ~Defines();

    virtual char const * const GetWindowClass() { return Defines::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Defines"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void DefineCreated(std::shared_ptr<Define> const&, bool);

    std::weak_ptr<System>            current_system;
    int selected_row;

    std::vector<std::weak_ptr<Define>> defines;
    bool force_reiterate;
    bool force_resort;

    bool case_sensitive_sort;

    //System::define_created_t::signal_connection_t define_created_connection;

public:
    static std::shared_ptr<Defines> CreateWindow();
};

} //namespace Windows

} //namespace NES
