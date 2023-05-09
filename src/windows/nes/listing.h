#pragma once

#include <memory>
#include <thread>

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
    void DisassemblyPopup();

private:
    NES::GlobalMemoryLocation selection;

    int    jump_to_selection    = 0;
    bool   create_new_label     = false;
    char   new_label_buffer[64] = "";

    bool   show_disassembling_popup = false;
    std::unique_ptr<std::thread> disassembly_thread;

public:
    static std::shared_ptr<Listing> CreateWindow();
};

}
