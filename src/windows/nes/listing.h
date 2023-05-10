#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class GlobalMemoryLocation;

class Listing : public BaseWindow {
public:
    Listing();
    virtual ~Listing();

    virtual char const * const GetWindowClass() { return Listing::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Listing"; }

    // signals
    // listing_command is used to trigger events that are not immediate - things like opening popups or asking the user
    // for information. other things the listing window does that are immediate (like changing memory object types) are
    // executed directly without the signal
    typedef signal<std::function<void(std::shared_ptr<BaseWindow> const&, std::string const& cmd, GlobalMemoryLocation const& where)>> listing_command_t;
    std::shared_ptr<listing_command_t> listing_command;

protected:
    void UpdateContent(double deltaTime) override;
    void PreRenderContent() override;
    void RenderContent() override;

private:
    void ClearForwardHistory();
    void CheckInput();

    void UserLabelCreated(GlobalMemoryLocation const&, std::string const&);
    void DisassemblyStopped(GlobalMemoryLocation const&);

private:
    GlobalMemoryLocation             current_selection;
    std::stack<GlobalMemoryLocation> selection_history_back;
    std::stack<GlobalMemoryLocation> selection_history_forward;

    bool   adjust_columns = false;

    int    jump_to_selection    = 0;
    char   new_label_buffer[64] = "";

    // signal connections
    System::user_label_created_t::signal_connection_t  user_label_created_connection;;
    System::disassembly_stopped_t::signal_connection_t disassembly_stopped_connection;
public:
    static std::shared_ptr<Listing> CreateWindow();
};

}
