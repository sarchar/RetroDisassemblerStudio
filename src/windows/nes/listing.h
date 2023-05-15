#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/base_window.h"

namespace NES {

class GlobalMemoryLocation;
class Label;

class Listing : public BaseWindow {
public:
    Listing();
    virtual ~Listing();

    virtual char const * const GetWindowClass() { return Listing::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Listing"; }

    void GoToAddress(GlobalMemoryLocation const&);
    void GoToAddress(u32);
    void Follow();

    // signals

protected:
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

private:
    void ClearForwardHistory();
    void CheckInput() override;

    void LabelCreated(std::shared_ptr<Label> const&, bool);
    void DisassemblyStopped(GlobalMemoryLocation const&);

private:
    std::weak_ptr<System>            current_system;
    GlobalMemoryLocation             current_selection;
    std::stack<GlobalMemoryLocation> selection_history_back;
    std::stack<GlobalMemoryLocation> selection_history_forward;

    bool   adjust_columns = false;

    int    jump_to_selection    = 0;
    char   new_label_buffer[64] = "";

    bool   editing = false;

    // signal connections
    System::label_created_t::signal_connection_t label_created_connection;
    System::disassembly_stopped_t::signal_connection_t disassembly_stopped_connection;

    struct {
        struct {
            std::string title;
            bool        show  = false;
            int         edit;
            std::string buf;
            GlobalMemoryLocation where;
        } create_label;

        struct {
            std::string title = "Disassembling...";
            std::shared_ptr<std::thread> thread = nullptr;
            bool        show  = false;
        } disassembly;

        struct {
            std::string title;
            bool        show  = false;
            std::string buf;
            MemoryObject::COMMENT_TYPE type;
            GlobalMemoryLocation where;
        } edit_comment;

        struct {
            std::string title = "Go to address...";
            std::string buf;
            bool        show = false;
        } goto_address;
    } popups;

    void RenderPopups();

public:
    static std::shared_ptr<Listing> CreateWindow();
};

}
