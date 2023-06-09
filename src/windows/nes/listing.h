// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <deque>
#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/memory.h" // TODO would be nice to switch current_selection to a shared_ptr so to eliminate this include
#include "windows/basewindow.h"

namespace Systems::NES {
    class Enum;
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
    static char const * const GetWindowClassStatic() { return "Windows::NES::Listing"; }
    static std::shared_ptr<Listing> CreateWindow();

    // save=true => save the current address in location history
    void GoToAddress(GlobalMemoryLocation const&, bool save);
    void GoToAddress(u32, bool save);
    void GoToCurrentInstruction();
    void Refocus(); // re focus on the current selection
    void Follow();
    void GoBack(); // go back in the location history
    void GoForward(); // go forward in the location history

    // signals

protected:
    void Update(double deltaTime) override;
    void Render() override;

private:
    void ClearForwardHistory();
    void CheckInput() override;
    void MoveSelectionUp();
    void MoveSelectionDown();
    void CreateDestinationLabel();

    void LabelCreated(std::shared_ptr<Label> const&, bool);
    void DisassemblyStopped(GlobalMemoryLocation const&);

private:
    std::shared_ptr<System>          current_system;
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
    bool   jump_to_pc           = false;

    char   new_label_buffer[64] = "";

    bool   editing_listing_item = false;

    // signal connections
    signal_connection label_created_connection;
    signal_connection disassembly_stopped_connection;
    signal_connection window_parented_connection;
    signal_connection breakpoint_hit_connection;

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
};

} // namespace Windows::NES

