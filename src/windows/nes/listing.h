#pragma once

#include <memory>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class Listing : public BaseWindow {
public:
    Listing();
    virtual ~Listing();

    virtual char const * const GetWindowClass() { return Listing::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Listing"; }

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void PreRenderContent() override;
    void RenderContent() override;

    void CheckInput();

private:
    void NewLabelPopup();

private:
    NES::GlobalMemoryLocation selection;

    int                       jump_to_selection    = 0;
    bool                      create_new_label     = false;
    char                      new_label_buffer[64] = "";

public:
    static std::shared_ptr<Listing> CreateWindow();
};

}
