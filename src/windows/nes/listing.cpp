// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>

#include "imgui.h"
#include "imgui_internal.h"

#include "util.h"

#include "systems/nes/cartridge.h"
#include "systems/nes/comment.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/system.h"

#include "windows/nes/emulator.h"
#include "windows/nes/enums.h"
#include "windows/nes/listing.h"
#include "windows/nes/listingitems.h"
#include "windows/nes/project.h"
#include "windows/nes/regions.h"

// Was 3, which was working nicely but increased by 1 because I added an extra frame to check
// if the selection is visible
#define JUMP_TO_SELECTION_START_VALUE 4

using namespace std;

namespace Windows::NES {

REGISTER_WINDOW(Listing);

shared_ptr<Listing> Listing::CreateWindow()
{
    return make_shared<Listing>();
}

Listing::Listing()
    : BaseWindow(), current_selection_listing_item(0)
{
    SetTitle("Listing");
    SetNoScrollbar(true);
    //SetNav(false); // disable navigation
    
    // create internal signals

    if(current_system = GetSystem()) {
        // after certain events, force the listing window back to where the cursor was
        label_created_connection = 
            current_system->label_created->connect(std::bind(&Listing::LabelCreated, this, placeholders::_1, placeholders::_2));

        disassembly_stopped_connection = 
            current_system->disassembly_stopped->connect(std::bind(&Listing::DisassemblyStopped, this, placeholders::_1));

        // my code is silly .. set breakpoint_hit_connection after we have a parent system instance
        window_parented_connection = window_parented->connect([&](shared_ptr<BaseWindow> const&) {
            breakpoint_hit_connection =
                GetMySystemInstance()->breakpoint_hit->connect([&](shared_ptr<BreakpointInfo> const&) {
                    // even on read/write to memory addresses, we want to look at where the breakpoint occured,
                    // not the address of the breakpoint itself
                    GoToCurrentInstruction();
                });
            });

        // initialize the first selection to the entry point of the program
        // TODO save last location as well as location history?
        current_system->GetEntryPoint(&current_selection);
        if(auto memory_object = current_system->GetMemoryObject(current_selection)) {
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
void Listing::Follow()
{
    if(auto memory_object = current_system->GetMemoryObject(current_selection)) {
        if(memory_object->operand_expression && memory_object->operand_expression->GetRoot()) {
            // look for a label
            bool found = false;
            memory_object->operand_expression->Explore([this, &found](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
                if(auto label_node = dynamic_pointer_cast<Systems::NES::ExpressionNodes::Label>(node)) {
                    // first label found, go to it
                    GoToAddress(label_node->GetTarget(), true);
                    found = true;
                    return false; // bail
                }
                return true; // keep hunting
            }, nullptr);

            // if no label was found, go to the evaluation
            if(!found) {
                s64 result;
                string errmsg;
                if(memory_object->operand_expression->Evaluate(&result, errmsg)) {
                    GoToAddress((u16)(result & 0xFFFF), true);
                }
            }
        } else if(memory_object->type == MemoryObject::TYPE_CODE) {
            u16 dest = 0;
            if(memory_object->GetSize() == 2) {
                dest = (u16)memory_object->data_ptr[1];
            } else if(memory_object->GetSize() == 3) {
                dest = (u16)memory_object->data_ptr[1] | ((u16)memory_object->data_ptr[2] << 8);
            }
            GoToAddress(dest, true);
        } else if(memory_object->type == MemoryObject::TYPE_WORD) {
            u16 hval = (u16)memory_object->data_ptr[0] | ((u16)memory_object->data_ptr[1] << 8);
            GoToAddress(hval, true);
        }
    }
}

void Listing::GoToCurrentInstruction()
{
    GetMySystemInstance()->GetCurrentInstructionAddress(&current_selection);

    if(auto memory_object = current_system->GetMemoryObject(current_selection)) {
        current_selection_listing_item = memory_object->primary_listing_item_index;
    }

    has_end_selection = false;

    Refocus();
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

    if(auto memory_object = current_system->GetMemoryObject(current_selection)) {
        current_selection_listing_item = memory_object->primary_listing_item_index;
    }

    jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
}

void Listing::GoToAddress(u32 address, bool save)
{
    auto memory_region = current_system->GetMemoryRegion(current_selection);

    if(address >= memory_region->GetBaseAddress() && address < memory_region->GetEndAddress()) {
        GlobalMemoryLocation new_selection(current_selection);
        new_selection.address = address;
        GoToAddress(new_selection, save);
    } else {
        // destination is not in this memory region, see if we can find it
        GlobalMemoryLocation guessed_address;
        guessed_address.address = address;

        if(current_system->CanBank(guessed_address)) {
            vector<u16> possible_banks;
            current_system->GetBanksForAddress(guessed_address, possible_banks);

            if(possible_banks.size() == 1) {
                guessed_address.prg_rom_bank = possible_banks[0];
                memory_region = current_system->GetMemoryRegion(guessed_address);
                if(memory_region) {
                    GoToAddress(guessed_address, save);
                }
            } else {
                auto wnd = MemoryRegions::CreateWindow(true, address);
                AddChildWindow(wnd);
                *wnd->region_selected += [this, address, save](shared_ptr<Systems::NES::MemoryRegion> const& memory_region) {
                    assert(address >= memory_region->GetBaseAddress() && address < memory_region->GetEndAddress());
                    GlobalMemoryLocation target;
                    memory_region->GetGlobalMemoryLocation(address - memory_region->GetBaseAddress(), &target);
                    GoToAddress(target, save);
                };
            }
        } else {
            // not a banked address, go to it if it's valid
            if(current_system->GetMemoryRegion(guessed_address)) GoToAddress(guessed_address, save);
        }
    }
}

void Listing::Update(double deltaTime) 
{
}

void Listing::ClearForwardHistory()
{
    while(selection_history_forward.size()) selection_history_forward.pop();
}

void Listing::MoveSelectionUp()
{
    auto memory_object = current_system->GetMemoryObject(current_selection);
    if(!memory_object) return;

    if(current_selection_listing_item == 0) {
        if(auto memory_object = current_system->GetMemoryObject(current_selection + -1)) {
            current_selection = current_selection + -memory_object->GetSize();
            current_selection_listing_item = memory_object->listing_items.size() - 1;
        }
    } else {
        current_selection_listing_item -= 1;
    }
}

void Listing::MoveSelectionDown()
{
    auto memory_object = current_system->GetMemoryObject(current_selection);
    if(!memory_object) return;

    current_selection_listing_item += 1;
    if(current_selection_listing_item >= memory_object->listing_items.size()) {
        current_selection = current_selection + memory_object->GetSize();
        current_selection_listing_item = 0;
    }
}

void Listing::GoBack()
{
    if(!selection_history_back.size()) return;

    selection_history_forward.push(current_selection);
    GlobalMemoryLocation dest = selection_history_back.top();
    selection_history_back.pop();
    GoToAddress(dest, false);
}

void Listing::GoForward()
{
    if(!selection_history_forward.size()) return;

    selection_history_back.push(current_selection);
    GlobalMemoryLocation dest = selection_history_forward.top();
    selection_history_forward.pop();
    GoToAddress(dest, false);
}

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    // don't process keypresses while editing a listing item
    if(editing_listing_item) return;

    bool no_mods = !(io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper);
    bool shift_only = !(io.KeyCtrl || io.KeyAlt || io.KeySuper) && io.KeyShift;

    if(no_mods) {
        // handle back mouse button
        if(ImGui::IsKeyPressed(ImGuiKey_MouseX1)) GoBack();

        // handle forward mouse button
        if(ImGui::IsKeyPressed(ImGuiKey_MouseX2)) GoForward();

        // Move selection up
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow) || ImGui::IsKeyPressed(ImGuiKey_K)) MoveSelectionUp();

        // Move selection down
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_J)) MoveSelectionDown();

        // Refocus window on current selection
        if(ImGui::IsKeyPressed(ImGuiKey_Tab)) Refocus();

        // start disassembly
        if(ImGui::IsKeyPressed(ImGuiKey_D)) {
            if(!popups.disassembly.thread) { // should never happen
                current_system->InitDisassembly(current_selection);
                popups.disassembly.thread = make_unique<std::thread>(std::bind(&System::DisassemblyThread, current_system));
                popups.disassembly.show = true;
                cout << "[Listing::CheckInput] started disassembly thread" << endl;
            }
        }

        // mark data as bytes
        if(ImGui::IsKeyPressed(ImGuiKey_B)) {
            if(int len = GetSelection(); len > 0) {
                current_system->MarkMemoryAsBytes(current_selection, len);
            }
        }

        // mark data as words
        if(ImGui::IsKeyPressed(ImGuiKey_W)) {
            if(int len = GetSelection(); len > 0) {
                current_system->MarkMemoryAsWords(current_selection, len);
            }
        }

        // mark data as a string
        if(ImGui::IsKeyPressed(ImGuiKey_S)) {
            if(int len = GetSelection(); len > 0) {
                current_system->MarkMemoryAsString(current_selection, len);
            }
        }

        // mark data as enum
        if(ImGui::IsKeyPressed(ImGuiKey_E)) {
            if(int len = GetSelection(); len > 0) {
                auto wnd = Enums::CreateWindow(true);
                AddChildWindow(wnd);
                // copy the selection
                auto system = current_system;
                auto selection = current_selection;
                *wnd->enum_selected += [system, selection, len](shared_ptr<Systems::NES::Enum> const& enum_type) {
                    system->MarkMemoryAsEnum(selection, len, enum_type);
                };
            }
        }

        // goto address
        if(ImGui::IsKeyPressed(ImGuiKey_G)) {
            popups.goto_address.show = true;
            popups.goto_address.buf = "";
        }

        // create or edit a label
        if(ImGui::IsKeyPressed(ImGuiKey_L)) {
            popups.create_label.buf = "";
            popups.create_label.show = true;
            popups.create_label.title = "Create new label";
            popups.create_label.where = current_selection;
        }

        // edit post comment
        if(ImGui::IsKeyPressed(ImGuiKey_O)) {
            popups.edit_comment.title = "Edit post-comment";
            popups.edit_comment.type  = MemoryObject::COMMENT_TYPE_POST;
            popups.edit_comment.show  = true;
            popups.edit_comment.where = current_selection;
            auto comment = current_system->GetComment(current_selection, popups.edit_comment.type);
            string ctext = "";
            if(comment) comment->GetFullCommentText(ctext);
            popups.edit_comment.buf = ctext;
        }

        // edit EOL comment
        if(ImGui::IsKeyPressed(ImGuiKey_Semicolon)) {
            popups.edit_comment.title = "Edit EOL-comment";
            popups.edit_comment.type  = MemoryObject::COMMENT_TYPE_EOL;
            popups.edit_comment.show  = true;
            popups.edit_comment.where = current_selection;
            auto comment = current_system->GetComment(current_selection, popups.edit_comment.type);
            string ctext = "";
            if(comment) comment->GetFullCommentText(ctext);
            popups.edit_comment.buf = ctext;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_P)) {
            CreateDestinationLabel();
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Minus)) {
            GetSystem()->RemoveBlankLine(current_selection);
        }
    } 

    if(shift_only) {
        if(ImGui::IsKeyPressed(ImGuiKey_F)) Follow();

        // edit pre comment
        if(ImGui::IsKeyPressed(ImGuiKey_O)) {
            popups.edit_comment.title = "Edit pre-comment";
            popups.edit_comment.type  = MemoryObject::COMMENT_TYPE_PRE;
            popups.edit_comment.show  = true;
            popups.edit_comment.where = current_selection;
            auto comment = current_system->GetComment(current_selection, popups.edit_comment.type);
            string ctext = "";
            if(comment) comment->GetFullCommentText(ctext);
            popups.edit_comment.buf = ctext;
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Equal)) {
            GetSystem()->AddBlankLine(current_selection);
        }
    }
}

void Listing::CreateDestinationLabel()
{
    auto memory_region = current_system->GetMemoryRegion(current_selection);
    if(!memory_region) return;

    auto memory_object = memory_region->GetMemoryObject(current_selection);
    if(!memory_object) return;

    // call CreateDefaultOperandExpression on CODE types
    if(memory_object->type == MemoryObject::TYPE_CODE) {
        auto det_func = [this](u32 address, System::finish_default_operand_expression_func finish_expression) { 
            auto wnd = MemoryRegions::CreateWindow(true, address);
            bool did_select = false;

            *wnd->region_selected += [address, finish_expression, &did_select](shared_ptr<Systems::NES::MemoryRegion> const& memory_region) {
                GlobalMemoryLocation target_location;
                memory_region->GetGlobalMemoryLocation(address - memory_region->GetBaseAddress(), &target_location);
                finish_expression(target_location);
                did_select = true;
            };

            *wnd->window_closed += [finish_expression, &did_select](shared_ptr<BaseWindow> const&) {
                // if window was closed without selecting a region, apply the expression without a label
                if(!did_select) finish_expression(nullopt);
            };

            AddChildWindow(wnd);
        };

        current_system->CreateDefaultOperandExpression(current_selection, true, det_func);
        return;
    }

    // otherwise, only create pointers from byte or word data
    if(memory_object->type != MemoryObject::TYPE_BYTE 
       && memory_object->type != MemoryObject::TYPE_WORD) return;

    auto apply_label = [this, memory_region, memory_object](GlobalMemoryLocation const& label_address) {
        // get the target location and create a label if none exists
        int offset = 0;

        // create a label at the target address. counterintuitively, this is not a "user created label"
        auto label = current_system->GetDefaultLabelForTarget(label_address, false, &offset, true, "L_");
        
        // now apply a OperandAddressOrLabel to the data on this memory object
        auto default_operand_format = memory_object->FormatOperandField(); // format the data as $xxxx
        auto expr = make_shared<Systems::NES::Expression>();
        auto nc = dynamic_pointer_cast<Systems::NES::ExpressionNodeCreator>(expr->GetNodeCreator());
        auto root = nc->CreateLabel(label_address, label->GetIndex(), default_operand_format);
        
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
    };

    // use the word data as a pointer to memory
    u16 address = (u16)memory_object->data_ptr[0];
    if(memory_object->type == MemoryObject::TYPE_WORD) {
        address |= ((u16)memory_object->data_ptr[1] << 8);
    }

    // set up a default address using the current bank/address
    GlobalMemoryLocation label_address(current_selection);
    label_address.address = address;

    if((address >= memory_region->GetBaseAddress() && address < memory_region->GetEndAddress())) {
        // target address is in the current bank
        apply_label(label_address);
    } else if(!current_system->CanBank(label_address)) {
        // not a banked address, apply label only if destination is valid
        if(current_system->GetMemoryObject(label_address)) apply_label(label_address);
    } else {
        // destination is bankable and not in this current memory region, 
        // try finding it or asking the user
        vector<u16> possible_banks;
        current_system->GetBanksForAddress(label_address, possible_banks); // only uses the address field

        if(possible_banks.size() == 1) {
            label_address.prg_rom_bank = possible_banks[0];
            apply_label(label_address);
        } else {
            auto wnd = MemoryRegions::CreateWindow(true, address);
            AddChildWindow(wnd);
            *wnd->region_selected += [address, apply_label](shared_ptr<Systems::NES::MemoryRegion> const& memory_region) {
                assert(address >= memory_region->GetBaseAddress() && address < memory_region->GetEndAddress());
                GlobalMemoryLocation label_address;
                memory_region->GetGlobalMemoryLocation(address - memory_region->GetBaseAddress(), &label_address);
                apply_label(label_address);
            };
        }
    }
}

void Listing::Render() 
{
    // postponed actions (things that change the listing display that cannot happen while rendering)
    Windows::NES::ListingItem::postponed_changes changes;

    bool focused = IsFocused();

    // reset the editing flag
    bool was_editing = editing_listing_item;
    editing_listing_item = false;

    // reset currently hovered item when window is not in focus
    if(!focused) hovered_listing_item_index = -1;

    if(ImGuiFlagButton(nullptr, "<", "Back (Alt-Left)")) GoBack();
    ImGui::SameLine(); 
    if(ImGuiFlagButton(nullptr, ">", "Forward (Alt-Right)")) GoForward();
    ImGui::SameLine(); 
    ImGuiFlagButton(&adjust_columns, "R", "Show Column Resizers");
    ImGui::Separator();

    // Let's not render content while disassembling
    if(!current_system->IsDisassembling()) {
        // Need the program rom bank that is currently in the listing
        auto memory_region = current_system->GetMemoryRegion(current_selection);

        // Grab the current CPU instruction address
        GlobalMemoryLocation pc_address;
        GetMySystemInstance()->GetCurrentInstructionAddress(&pc_address);

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
            // we have a buffer of 25 lines before and after so ImGui can calculate row heights
            // but don't do it on the first frame so we get a chance to see if the object is already
            // visible
            u32 listing_item_index = memory_region->GetListingIndexByAddress(current_selection) + current_selection_listing_item;
            if(jump_to_selection > 0 && jump_to_selection != JUMP_TO_SELECTION_START_VALUE) {
                clipper.ForceDisplayRangeByIndices(listing_item_index - 25, listing_item_index + 25);
            }

            // determine the ending item index (which may be before the start, since we can select upwards)
            u32 end_listing_item_index = 0;
            if(has_end_selection) {
                end_listing_item_index = memory_region->GetListingIndexByAddress(end_selection) + end_selection_listing_item;
                // swap if so
                if(end_listing_item_index < listing_item_index) swap(listing_item_index, end_listing_item_index);
            }

            auto system_instance = GetMySystemInstance();
            bool did_scroll = false;
            while(clipper.Step()) {
                auto listing_item_iterator = memory_region->GetListingItemIterator(clipper.DisplayStart);

                //cout << "DisplayStart = " << hex << clipper.DisplayStart << " - " << clipper.DisplayEnd << endl;
                for(int row = clipper.DisplayStart; row < clipper.DisplayEnd && listing_item_iterator; ++row, ++*listing_item_iterator) {
                    // get the listing item
                    auto listing_item = listing_item_iterator->GetListingItem();

                    // Get the address this listing_item belongs to so we can highlight it when selected
                    GlobalMemoryLocation current_address(current_selection); // start with a copy since we're in the same memory region
                    current_address.address = listing_item_iterator->GetCurrentAddress();

                    //cout << "row = 0x" << row << " current_address.address = 0x" << hex << current_address.address << endl;

                    // selected and hovered let the listing item know how to behave wrt to inputs
                    bool selected = has_end_selection ? (row >= listing_item_index && row <= end_listing_item_index) : (listing_item_index == row);
                    bool hovered  = (hovered_listing_item_index == row) && !was_editing; // Don't show hovered items when editing something

                    // only Primary items are highlightable for the debugger
                    bool is_primary = (bool)dynamic_pointer_cast<ListingItemPrimary>(listing_item);
                    bool at_pc    = (current_address == pc_address) && is_primary;

                    // Begin a new row and next column
                    ImGui::TableNextRow();

                    // set the background color
                    auto row_color_type = ImGuiCol_Header;
                    if(hovered) row_color_type = ImGuiCol_HeaderHovered;

                    ImU32 row_color;
                    if(at_pc && is_primary) row_color = IM_COL32(232, 217, 132, 200);
                    else row_color = ImGui::GetColorU32(row_color_type);

                    if(selected || hovered || at_pc) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, row_color);

                    ImGui::TableNextColumn(); // start the content of the listing item
                    listing_item->Render(system_instance, current_system, current_address, adjust_columns, focused, selected, hovered, changes); // render the content
                    bool item_visible = ImGui::IsItemVisible();
                    bool item_hovered = ImGui::IsItemHovered();

                    // if the item has determined to be editing something, take note
                    if(listing_item->IsEditing()) {
                        editing_listing_item = true;
                        // if you enter into editing mode, no way can you stay in multiple selections
                        // so the first listing item that (i.e., the current_selection) will be editing and the rest will not
                        has_end_selection = false;
                    }

                    // update the currently hovered and selected row
                    if(item_hovered) {
                        hovered_listing_item_index = row;

                        // if the mouse was clicked on this row, change the selection as well
                        if(ImGui::IsMouseClicked(0)) {
                            if(ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift)) {
                                // select range 
                                end_selection = current_address;
                                end_selection_listing_item = listing_item_iterator->GetListingItemIndex();
                                if(!(current_selection == end_selection)) {
                                    has_end_selection = true;

                                    // end listing item has to be updated immediately but might be before the current_selection
                                    end_listing_item_index = memory_region->GetListingIndexByAddress(end_selection) + end_selection_listing_item;
                                    // swap if so
                                    if(end_listing_item_index < listing_item_index) swap(listing_item_index, end_listing_item_index);
                                }
                            } else {
                                current_selection = current_address;
                                current_selection_listing_item = listing_item_iterator->GetListingItemIndex();
                                has_end_selection = false;
                            }
                        }
                    }

                    // Only after the row has been rendered and it was the last element in the table,
                    // we can use ScrollToItem() to get the item focused in the middle of the view.
                    if(jump_to_selection > 0 && current_address.address == current_selection.address && !did_scroll) {
                        // short circuit the jump if the item is currently visible
                        if(item_visible) {
                            jump_to_selection = 0;
                        } else {
                            ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY);
                            did_scroll = true;
                        }

                        jump_to_selection -= 1;
                    }
                }
            }
            ImGui::EndTable();

            // when the target is offscreen, the first frame of jump_to_selection won't scroll
            if(jump_to_selection == JUMP_TO_SELECTION_START_VALUE && !did_scroll) jump_to_selection -= 1;
        }

        ImGui::PopStyleVar(2);
    }

    RenderPopups();

    // any changes to listing items can now be applied
    while(changes.size()) {
        std::function<void()> func = changes.front();
        changes.pop_front();
        func();
    }
}

// If the end_selection address comes before the current_selection address, they need to be swapped
// and then we should return the distance between the two addresses (+ length of the end object)
int Listing::GetSelection()
{
    if(!has_end_selection) { // no selection, just return the size of the current object
        if(auto memory_object = current_system->GetMemoryObject(current_selection)) {
            return memory_object->GetSize();
        } else {
            return 0;
        }
    }

    if(end_selection.address < current_selection.address) {
        swap(current_selection, end_selection);
        swap(current_selection_listing_item, end_selection_listing_item);
    }

    if(auto end_object = current_system->GetMemoryObject(end_selection)) {
        return (end_selection.address - current_selection.address) + end_object->GetSize();
    }

    // Bogus end address?
    return 0;
}

void Listing::RenderPopups() 
{
    int ret;

    if(popups.create_label.show 
            && (ret = GetMainWindow()->InputNamePopup(popups.create_label.title, "Label", &popups.create_label.buf)) != 0) {
        if(ret > 0) {
            if(popups.create_label.buf.size() > 0) {
                //TODO verify valid label (no spaces, etc)
                // dialog was OK'd, add the label
                current_system->CreateLabel(popups.create_label.where, popups.create_label.buf, true);
            }
        }
        popups.create_label.show = false;
    }

    if(popups.disassembly.show
            && (ret = GetMainWindow()->WaitPopup(popups.disassembly.title, "Disassembling...", !current_system->IsDisassembling())) != 0) {
        if(popups.disassembly.thread) {
            popups.disassembly.thread->join();
            popups.disassembly.thread = nullptr;
            cout << "[Listing::DisassemblyPopup] disassembly thread exited" << endl;
        }
        popups.disassembly.show = false;
    }

    if(popups.edit_comment.show 
            && (ret = GetMainWindow()->InputMultilinePopup(popups.edit_comment.title, "Comment", &popups.edit_comment.buf)) != 0) {
        if(ret > 0) {
            if(popups.edit_comment.buf.size() > 0) {
                auto comment = make_shared<Systems::NES::Comment>();
                comment->Set(popups.edit_comment.buf);
                cout << "Setting comment:" << endl << *comment;
                current_system->SetComment(popups.edit_comment.where, popups.edit_comment.type, comment);
            }
        }
        popups.edit_comment.show = false;
    }

    if(popups.goto_address.show 
            && (ret = GetMainWindow()->InputHexPopup(popups.goto_address.title, "Address (hex)", &popups.goto_address.buf)) != 0) {
        if(ret > 0) {
            if(popups.goto_address.buf.size() > 0) {
                // parse the address given (TODO verify it's valid?)
                stringstream ss;
                ss << hex << popups.goto_address.buf;
                u32 address;
                ss >> address;
                
                GoToAddress(address, true);
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

} // namespace Windows::NES

