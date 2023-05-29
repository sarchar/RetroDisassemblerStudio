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
    static char const * const GetWindowClassStatic() { return "NES::Defines"; }

    void Highlight(std::shared_ptr<Systems::NES::Define>&);

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    void DefineCreated(std::shared_ptr<Systems::NES::Define> const&);

    std::weak_ptr<Systems::NES::System> current_system;
    int selected_row;
    int context_row = 0;

    std::vector<std::weak_ptr<Systems::NES::Define>> defines;
    bool force_reiterate;
    bool force_resort;

    bool case_sensitive_sort;

    std::shared_ptr<Systems::NES::Define> highlight;

    signal_connection define_created_connection;

public:
    static std::shared_ptr<Defines> CreateWindow();
};

} //namespace NES

} //namespace Windows
