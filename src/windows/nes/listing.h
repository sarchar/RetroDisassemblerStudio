#pragma once

#include <memory>
#include <stack>

#include "signals.h"
#include "systems/nes/nes_system.h"
#include "windows/basewindow.h"

namespace NES {

class GlobalMemoryLocation;
class Label;

namespace Windows {

class Listing : public BaseWindow {
public:
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
    void UpdateContent(double deltaTime) override;
    void RenderContent() override;

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
    std::stack<GlobalMemoryLocation> selection_history_back;
    std::stack<GlobalMemoryLocation> selection_history_forward;

    bool   adjust_columns = false;

    int    jump_to_selection    = 0;
    char   new_label_buffer[64] = "";

    bool   editing_listing_item = false;

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

} // namespace Windows

} // namespace NES
