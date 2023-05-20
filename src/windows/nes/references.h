#pragma once

#pragma once

#include <memory>
#include <stack>
#include <variant>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/basewindow.h"

namespace NES {

class Define;
class GlobalMemoryLocation;
class Label;

namespace Windows {

class References : public BaseWindow {
public:
    typedef std::variant<
        GlobalMemoryLocation,
        std::shared_ptr<Define>,
        std::shared_ptr<Label>> reference_type;

    References(reference_type const&);
    virtual ~References();

    virtual char const * const GetWindowClass() { return References::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::References"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    std::weak_ptr<System>            current_system;
    reference_type                   reference_to;

    int selected_row;
    bool force_resort     = true;
    bool force_repopulate = true;

    std::shared_ptr<signal_connection_base> changed_connection;

    typedef std::variant<
        GlobalMemoryLocation,
        std::shared_ptr<Define>
    > location_type;
    std::vector<location_type> locations;

    void PopulateLocations();
    void PopulateDefineLocations(std::shared_ptr<Define>&);
    void PopulateLabelLocations(std::shared_ptr<Label>&);

public:
    static std::shared_ptr<References> CreateWindow(reference_type const&);
};

} //namespace Windows

} //namespace NES
