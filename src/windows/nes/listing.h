#pragma once

#include <deque>
#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_memory.h" // TODO would be nice to switch current_selection to a shared_ptr so to eliminate this include
#include "windows/basewindow.h"

namespace Systems::NES {
    class GlobalMemoryLocation;
    class Label;
    class System;
}

namespace Windows::NES {

class Listing : public BaseWindow {
public:
    using GlobalMemoryLocation = Systems::NES::GlobalMemoryLocation;
    using Label                = Systems::NES::Label;
    using MemoryObject         = Systems::NES::MemoryObject;
    using System               = Systems::NES::System;

    Listing();
    virtual ~Listing();

    virtual char const * const GetWindowClass() { return Listing::GetWindowClassStatic(); }
    static char const * const GetWindowClassStatic() { return "NES::Listing"; }

    void GoToAddress(GlobalMemoryLocation const&, bool save = true);
    void GoToAddress(u32);
    void Refocus(); // re focus on the current selection
    void Follow();

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    void ClearForwardHistory();
    void CheckInput() override;
    void MoveSelectionUp();
    void MoveSelectionDown();

    void LabelCreated(std::shared_ptr<Label> const&, bool);
    void DisassemblyStopped(GlobalMemoryLocation const&);

private:
    std::weak_ptr<System>            current_system;
    GlobalMemoryLocation             current_selection;
    int                              current_selection_listing_item;
    int                              hovered_listing_item_index;
    bool                             has_end_selection = false;
    GlobalMemoryLocation             end_selection;
    int                              end_selection_listing_item;
    std::stack<GlobalMemoryLocation> selection_history_back;
    std::stack<GlobalMemoryLocation> selection_history_forward;

    int    GetSelection();

    bool   adjust_columns = false;

    int    jump_to_selection    = 0;
    char   new_label_buffer[64] = "";

    bool   editing_listing_item = false;

    // signal connections
    signal_connection label_created_connection;
    signal_connection disassembly_stopped_connection;

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

} // namespace Windows::NES

