// TODO in the future will need a way to make windows only available to the currently loaded system
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "windows/nes/listing.h"
#include "systems/nes/nes_cartridge.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"

#define JUMP_TO_SELECTION_START_VALUE 3

using namespace std;

namespace NES {

namespace Windows {

shared_ptr<Listing> Listing::CreateWindow()
{
    return make_shared<Listing>();
}

Listing::Listing()
    : BaseWindow("NES::Listing"), current_selection_listing_item(0)
{
    SetTitle("Listing");
    SetNoScrollbar(true);
    //SetNav(false); // disable navigation
    
    // create internal signals

    if(auto system = (MyApp::Instance()->GetProject()->GetSystem<System>())) {
        // grab a weak_ptr so we don't have to continually use dynamic_pointer_cast
        current_system = system;

        // after certain events, force the listing window back to where the cursor was
        label_created_connection = 
            system->label_created->connect(std::bind(&Listing::LabelCreated, this, placeholders::_1, placeholders::_2));

        disassembly_stopped_connection = 
            system->disassembly_stopped->connect(std::bind(&Listing::DisassemblyStopped, this, placeholders::_1));

        // initialize the first selection to the entry point of the program
        // TODO save last location as well as location history?
        system->GetEntryPoint(&current_selection);
        if(auto memory_object = system->GetMemoryObject(current_selection)) {
            current_selection_listing_item = memory_object->primary_listing_item_index;
        }
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE; // TODO this is stupid, I wish I could scroll within one or 
                                                           // two frames (given we have to calculate the row sizes at least once)
    }

    cout << "[NES::Listing] Program entry point at " << current_selection << endl;
}

Listing::~Listing()
{
}

// Try following the operand parameter to its destination
// TODO this should be way more complicated, parsing operand expression and all
// TODO actually, can't we just evaluate the operand expression?
void Listing::Follow()
{
    if(auto system = current_system.lock()) {
        if(auto memory_object = system->GetMemoryObject(current_selection)) {
            u16 dest = 0;
            if(memory_object->type == MemoryObject::TYPE_CODE) {
                dest = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
            } else if(memory_object->type == MemoryObject::TYPE_WORD) {
                dest = memory_object->hval;
            }

            GoToAddress(dest);
        }
    }
}

void Listing::Refocus()
{
    jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
}

void Listing::GoToAddress(GlobalMemoryLocation const& address, bool save)
{
    if(save) {
        selection_history_back.push(current_selection); // save the current location to the history
        ClearForwardHistory();                          // and clear the forward history
    }
    current_selection = address;

    if(auto system = current_system.lock()) {
        if(auto memory_object = system->GetMemoryObject(current_selection)) {
            current_selection_listing_item = memory_object->primary_listing_item_index;
        }
    }

    jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
}

void Listing::GoToAddress(u32 address)
{
    auto system = current_system.lock();
    if(!system) return;

    auto memory_region = system->GetMemoryRegion(current_selection);

    if(address >= memory_region->GetBaseAddress() && address < memory_region->GetEndAddress()) {
        GlobalMemoryLocation new_selection(current_selection);
        new_selection.address = address;
        GoToAddress(new_selection);
    } else {
        // destination is not in this memory region, see if we can find it
        GlobalMemoryLocation guessed_address;
        guessed_address.address = address;

        if(system->CanBank(guessed_address)) {
            vector<u16> possible_banks;
            system->GetBanksForAddress(guessed_address, possible_banks);

            if(possible_banks.size() == 1) {
                guessed_address.prg_rom_bank = possible_banks[0];
                memory_region = system->GetMemoryRegion(guessed_address);
                if(memory_region) {
                    GoToAddress(guessed_address);
                }
            } else {
                assert(false); // popup dialog and wait for selection of which bank to go to
            }
        } else {
            // not a banked address, go to it if it's valid
            if(system->GetMemoryRegion(guessed_address)) GoToAddress(guessed_address);
        }
    }
}

void Listing::UpdateContent(double deltaTime) 
{
}

void Listing::ClearForwardHistory()
{
    while(selection_history_forward.size()) selection_history_forward.pop();
}

void Listing::MoveSelectionUp()
{
    auto system = current_system.lock();
    if(!system) return;

    auto memory_object = system->GetMemoryObject(current_selection);
    if(!memory_object) return;

    if(current_selection_listing_item == 0) {
        if(auto memory_object = system->GetMemoryObject(current_selection + -1)) {
            current_selection = current_selection + -memory_object->GetSize();
            current_selection_listing_item = memory_object->listing_items.size() - 1;
        }
    } else {
        current_selection_listing_item -= 1;
    }
}

void Listing::MoveSelectionDown()
{
    auto system = current_system.lock();
    if(!system) return;

    auto memory_object = system->GetMemoryObject(current_selection);
    if(!memory_object) return;

    current_selection_listing_item += 1;
    if(current_selection_listing_item >= memory_object->listing_items.size()) {
        current_selection = current_selection + memory_object->GetSize();
        current_selection_listing_item = 0;
    }
}

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    auto system = current_system.lock();
    if(!system) return;

    // don't process keypresses while editing a listing item
    if(editing_listing_item) return;

    // handle back button
    if(ImGui::IsKeyPressed(ImGuiKey_MouseX1) && selection_history_back.size()) {
        selection_history_forward.push(current_selection);
        GlobalMemoryLocation dest = selection_history_back.top();
        selection_history_back.pop();
        GoToAddress(dest);
    }

    // handle forward button
    if(ImGui::IsKeyPressed(ImGuiKey_MouseX2) && selection_history_forward.size()) {
        selection_history_back.push(current_selection);
        GlobalMemoryLocation dest = selection_history_forward.top();
        selection_history_forward.pop();
        GoToAddress(dest);
    }

    // handle delete button
    if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        // TODO open dialog and ask what to clear - data type, labels, etc
        system->MarkMemoryAsUndefined(current_selection);
    }

    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) MoveSelectionUp();

    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) MoveSelectionDown();

    if(ImGui::IsKeyDown(ImGuiKey_Tab) && !(io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper)) {
        Refocus();
    }
 
    for(int i = 0; i < io.InputQueueCharacters.Size; i++) { 
        ImWchar c = io.InputQueueCharacters[i]; 

        // TODO really should be using signals and emit to broadcast these messages
        switch(c) {
        case L'j': // move current_selection down
            MoveSelectionDown();
            break;

        case L'k': // move current_selection up
            MoveSelectionUp();
            break;

        case L'w': // mark data as a word
            system->MarkMemoryAsWords(current_selection, 2);
            break;

        case L'l': // create or edit a label
        {
            popups.create_label.buf = "";
            popups.create_label.show = true;
            popups.create_label.title = "Create new label";
            popups.create_label.where = current_selection;
            break;
        }

        case L'O': // edit pre comment
        {
            popups.edit_comment.title = "Edit pre-comment";
            popups.edit_comment.type  = MemoryObject::COMMENT_TYPE_PRE;
            popups.edit_comment.show  = true;
            popups.edit_comment.where = current_selection;
            system->GetComment(current_selection, popups.edit_comment.type, popups.edit_comment.buf);
            break;
        }

        case L'o': // edit post comment
        {
            popups.edit_comment.title = "Edit post-comment";
            popups.edit_comment.type  = MemoryObject::COMMENT_TYPE_POST;
            popups.edit_comment.show  = true;
            popups.edit_comment.where = current_selection;
            system->GetComment(current_selection, popups.edit_comment.type, popups.edit_comment.buf);
            break;
        }

        case L'd': // start disassembly
        {
            if(!popups.disassembly.thread) { // should never happen
                system->InitDisassembly(current_selection);
                popups.disassembly.thread = make_unique<std::thread>(std::bind(&System::DisassemblyThread, system));
                popups.disassembly.show = true;
                cout << "[Listing::CheckInput] started disassembly thread" << endl;
            }
            break;
        }

        case L'g': // go to address
            popups.goto_address.show = true;
            popups.goto_address.buf = "";
            break;

        case L'F': // very hacky (F)ollow address button
        {
            Follow();
            break;
        }

        case L'p': // create pointer at cursor (apply a label to the address pointed by the word at this location)
        {
            if(auto memory_region = system->GetMemoryRegion(current_selection)) {
                if(auto memory_object = memory_region->GetMemoryObject(current_selection)) {
                    // only create pointers from word data
                    if(memory_object->type != MemoryObject::TYPE_WORD) break;

                    // use the word data as a pointer to memory
                    u16 target = memory_object->hval;

                    // set up a default address using the current bank/address
                    GlobalMemoryLocation label_address(current_selection);
                    label_address.address = target;

                    if(!(target >= memory_region->GetBaseAddress() && target < memory_region->GetEndAddress())) {
                        // destination is not in this memory region, see if we can find it
                        vector<u16> possible_banks;
                        system->GetBanksForAddress(label_address, possible_banks); // only uses the address field

                        if(possible_banks.size() == 1) {
                            label_address.prg_rom_bank = possible_banks[0];
                            memory_region = system->GetMemoryRegion(label_address);
                        } else {
                            cout << "[Listing::CheckInput] TODO: popup dialog asking for which bank this should point to" << endl;
                            break;
                        }
                    }

                    // get the target location and create a label if none exists
                    int offset = 0;

                    // counterintuitively, this is not a "user created label"
                    auto label = system->GetDefaultLabelForTarget(label_address, false, &offset, true, "L_");

                    // now apply a OperandAddressOrLabel to the data on this memory object
                    auto default_operand_format = memory_object->FormatOperandField(); // will format the data $xxxx
                    auto expr = make_shared<Expression>();
                    auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());
                    auto root = nc->CreateLabel(label, default_operand_format);

                    // if offset is nonzero, create an add offset expression
                    if(offset != 0) {
                        stringstream ss;
                        ss << offset;
                        auto offset_node = nc->CreateConstant(offset, ss.str());
                        root = nc->CreateAddOp(root, "+", offset_node);
                    }

                    // set the root node in the expression
                    expr->Set(root);

                    // set the expression for memory object at current_selection. it'll show up immediately
                    memory_region->SetOperandExpression(current_selection, expr);
                }
            }
            break;
        }

        default:
            break;

        }
    }
   
}

void Listing::RenderContent() 
{
    // All access goes through the system
    auto system = current_system.lock();
    if(!system) return;

    // reset the editing flag
    bool was_editing = editing_listing_item;
    editing_listing_item = false;

    {
        bool need_pop = false;
        if(adjust_columns) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
            need_pop = true;
        }

        if(ImGui::SmallButton("R")) adjust_columns = !adjust_columns;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show Column Resizers");

        if(need_pop) ImGui::PopStyleColor(1);

        ImGui::Separator();
    }

    // Let's not render content while disassembling
    if(!system->IsDisassembling()) {
        // Need the program rom bank that is currently in the listing
        auto memory_region = system->GetMemoryRegion(current_selection);

        ImGuiTableFlags outer_table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

        // We use nested tables so that each row can have its own layout. This will be useful when we can render
        // things like plate comments, labels, etc
        if(ImGui::BeginTable("listing_table", 1, outer_table_flags)) {
            ImGui::TableSetupColumn("RowContent", ImGuiTableColumnFlags_WidthStretch);

            ImGuiListClipper clipper;
            u32 total_listing_items = memory_region->GetTotalListingItems();
            //cout << "total_listing_items = 0x" << hex << total_listing_items << endl;
            clipper.Begin(total_listing_items);
            
            // Force the clipper to include a range that also includes the row we want to jump to
            // we have a buffer of 100 lines so ImGui can calculate row heights
            u32 listing_item_index = memory_region->GetListingIndexByAddress(current_selection) + current_selection_listing_item;
            if(jump_to_selection > 0) {
                clipper.ForceDisplayRangeByIndices(listing_item_index - 25, listing_item_index + 25);
            }

            while(clipper.Step()) {
                auto listing_item_iterator = memory_region->GetListingItemIterator(clipper.DisplayStart);
                bool did_scroll = false;

                //cout << "DisplayStart = " << hex << clipper.DisplayStart << " - " << clipper.DisplayEnd << endl;
                for(int row = clipper.DisplayStart; row < clipper.DisplayEnd && listing_item_iterator; ++row, ++*listing_item_iterator) {
                    // get the listing item
                    auto& listing_item = listing_item_iterator->GetListingItem();

                    // Get the address this listing_item belongs to so we can highlight it when selected
                    GlobalMemoryLocation current_address(current_selection); // start with a copy since we're in the same memory region
                    current_address.address = listing_item_iterator->GetCurrentAddress();

                    //cout << "row = 0x" << row << " current_address.address = 0x" << hex << current_address.address << endl;

                    // selected and hovered let the listing item know how to behave wrt to inputs
                    bool selected = (listing_item_index == row);
                    bool hovered  = (hovered_listing_item_index == row) && !was_editing; // Don't show hovered items when editing something

                    // Begin a new row and next column
                    ImGui::TableNextRow();

                    // set the background color
                    ImU32 const row_color = ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Header);
                    if(selected || hovered) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_color);

                    ImGui::TableNextColumn(); // start the content of the listing item
                    listing_item->RenderContent(system, current_address, adjust_columns, selected, hovered); // render the content

                    // if the item has determined to be editing something, take note
                    if(listing_item->IsEditing()) editing_listing_item = true;

                    // update the currently hovered and selected row
                    if(ImGui::IsItemHovered()) {
                        hovered_listing_item_index = row;

                        // if the mouse was clicked on this row, change the selection as well
                        if(ImGui::IsMouseClicked(0)) {
                            current_selection = current_address;
                            current_selection_listing_item = listing_item_iterator->GetListingItemIndex();
                        }
                    }

                    // Only after the row has been rendered and it was the last element in the table,
                    // we can use ScrollToItem() to get the item focused in the middle of the view.
                    if(jump_to_selection > 0 && current_address.address == current_selection.address && !did_scroll) {
                        ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
                        jump_to_selection -= 1;
                        did_scroll = true;
                    }
                }
            }
            ImGui::EndTable();
        }

        ImGui::PopStyleVar(2);
    }

    RenderPopups();
}

void Listing::RenderPopups() 
{
    int ret;

    auto system = current_system.lock();
    if(!system) return;

    if(popups.create_label.show 
            && (ret = MyApp::Instance()->InputNamePopup(popups.create_label.title, "Label", &popups.create_label.buf)) != 0) {
        if(ret > 0) {
            if(popups.create_label.buf.size() > 0) {
                //TODO verify valid label (no spaces, etc)
                // dialog was OK'd, add the label
                system->CreateLabel(popups.create_label.where, popups.create_label.buf, true);
            }
        }
        popups.create_label.show = false;
    }

    if(popups.disassembly.show
            && (ret = MyApp::Instance()->WaitPopup(popups.disassembly.title, "Disassembling...", !system->IsDisassembling())) != 0) {
        if(popups.disassembly.thread) {
            popups.disassembly.thread->join();
            popups.disassembly.thread = nullptr;
            cout << "[Listing::DisassemblyPopup] disassembly thread exited" << endl;
        }
        popups.disassembly.show = false;
    }

    if(popups.edit_comment.show 
            && (ret = MyApp::Instance()->InputMultilinePopup(popups.edit_comment.title, "Comment", &popups.edit_comment.buf)) != 0) {
        if(ret > 0) {
            if(popups.edit_comment.buf.size() > 0) {
                system->SetComment(popups.edit_comment.where, popups.edit_comment.type, popups.edit_comment.buf);
            } else {
                cout << "TODO: delete comment";
            }
        }
        popups.edit_comment.show = false;
    }

    if(popups.goto_address.show 
            && (ret = MyApp::Instance()->InputHexPopup(popups.goto_address.title, "Address (hex)", &popups.goto_address.buf)) != 0) {
        if(ret > 0) {
            if(popups.goto_address.buf.size() > 0) {
                // parse the address given (TODO verify it's valid?)
                stringstream ss;
                ss << hex << popups.goto_address.buf;
                u32 address;
                ss >> address;
                
                GoToAddress(address);
            }
        }
        popups.goto_address.show = false;
    }
}

void Listing::LabelCreated(shared_ptr<Label> const& label, bool was_user_created)
{
    if(!was_user_created) return;
    current_selection = label->GetMemoryLocation();
    jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
}

void Listing::DisassemblyStopped(GlobalMemoryLocation const& start_location)
{
    current_selection = start_location;
    jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
}

} // namespace Windows
} // namespace NES
