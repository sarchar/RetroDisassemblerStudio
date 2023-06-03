#pragma once

#pragma once

#include <memory>
#include <stack>
#include <variant>

#include "signals.h"
#include "windows/basewindow.h"

#include "systems/nes/memory.h"

namespace Systems::NES {

class Define;
class Label;
class System;

}

namespace Windows::NES {

class References : public BaseWindow {
public:
    using Define               = Systems::NES::Define;
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using Label                = Systems::NES::Label;
    using System               = Systems::NES::System;

    typedef std::variant<
        GlobalMemoryLocation,
        std::shared_ptr<Define>,
        std::shared_ptr<Label>> reference_type;

    References();
    References(reference_type const&);
    virtual ~References();

    virtual char const * const GetWindowClass() { return References::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "Windows::NES::References"; }
    static std::shared_ptr<References> CreateWindow();
    static std::shared_ptr<References> CreateWindow(reference_type const&);

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    std::weak_ptr<System>            current_system;
    reference_type                   reference_to;

    int selected_row;
    bool force_resort     = true;
    bool force_repopulate = true;

    signal_connection changed_connection;
    signal_connection label_deleted_connection;

    typedef std::variant<
        GlobalMemoryLocation,
        std::shared_ptr<Define>
    > location_type;
    std::vector<location_type> locations;

    void PopulateLocations();
    void PopulateDefineLocations(std::shared_ptr<Define>&);
    void PopulateLabelLocations(std::shared_ptr<Label>&);
};

} //namespace Windows::NES

