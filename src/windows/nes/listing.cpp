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
#include "systems/nes/nes_system.h"

#define JUMP_TO_SELECTION_START_VALUE 3

using namespace std;

namespace NES {

shared_ptr<Listing> Listing::CreateWindow()
{
    return make_shared<Listing>();
}

Listing::Listing()
    : BaseWindow("NES::Listing")
{
    SetTitle("Listing");
    SetNav(false); // disable navigation
    
    // create internal signals
    listing_command = make_shared<listing_command_t>();

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
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE; // TODO this is stupid, I wish I could scroll within one or 
                                                           // two frames (given we have to calculate the row sizes at least once)
    }

    cout << "[NES::Listing] Program entry point at " << current_selection << endl;
}

Listing::~Listing()
{
}

void Listing::GoToAddress(GlobalMemoryLocation const& address)
{
    selection_history_back.push(current_selection); // save the current location to the history
    ClearForwardHistory();                          // and clear the forward history
    current_selection = address;
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

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    auto system = current_system.lock();
    if(!system) return;

    // handle back button
    if(ImGui::IsKeyPressed(ImGuiKey_MouseX1) && selection_history_back.size()) {
        selection_history_forward.push(current_selection);
        current_selection = selection_history_back.top();
        selection_history_back.pop();
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
    }

    // handle forward button
    if(ImGui::IsKeyPressed(ImGuiKey_MouseX2) && selection_history_forward.size()) {
        selection_history_back.push(current_selection);
        current_selection = selection_history_forward.top();
        selection_history_forward.pop();
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
    }

    // handle delete button
    if(ImGui::IsKeyPressed(ImGuiKey_Delete)) {
        // TODO open dialog and ask what to clear - data type, labels, etc
        system->MarkMemoryAsUndefined(current_selection);
    }

    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        if(auto memory_object = system->GetMemoryObject(current_selection + -1)) {
            current_selection = current_selection + -memory_object->GetSize();
        }
    }

    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        if(auto memory_object = system->GetMemoryObject(current_selection)) {
            current_selection = current_selection + memory_object->GetSize();
        }
    }

    for(int i = 0; i < io.InputQueueCharacters.Size; i++) { 
        ImWchar c = io.InputQueueCharacters[i]; 

        // TODO really should be using signals and emit to broadcast these messages
        switch(c) {
        case L'j': // move current_selection down
            if(auto memory_object = system->GetMemoryObject(current_selection)) {
                current_selection = current_selection + memory_object->GetSize();
            }
            break;

        case L'k': // move current_selection up
            if(auto memory_object = system->GetMemoryObject(current_selection + -1)) {
                current_selection = current_selection + -memory_object->GetSize();
            }
            break;

        case L'w': // mark data as a word
            system->MarkMemoryAsWords(current_selection, 2);
            break;

        case L'l': // create a label
            listing_command->emit(shared_from_this(), "CreateLabel", current_selection);
            break;

        case L';': // edit EOL comment
            listing_command->emit(shared_from_this(), "EditEOLComment", current_selection);
            break;

        case L':': // edit pre comment
            listing_command->emit(shared_from_this(), "EditPreComment", current_selection);
            break;

        case L'o': // edit post comment
            listing_command->emit(shared_from_this(), "EditPostComment", current_selection);
            break;

        case L'd': // start disassembly
            listing_command->emit(shared_from_this(), "DisassemblyRequested", current_selection);
            break;

        case L'g': // go to address
            listing_command->emit(shared_from_this(), "GoToAddress", current_selection);
            break;

        case L'p': // create pointer at cursor (apply a label to the address pointed by the word at this location)
        {
            // TODO this will be a larger task in the future
            auto memory_region = system->GetMemoryRegion(current_selection);
            auto memory_object = memory_region->GetMemoryObject(current_selection);

            u16 dest = 0;
            if(memory_object->type == MemoryObject::TYPE_CODE) {
                dest = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
            } else if(memory_object->type == MemoryObject::TYPE_WORD) {
                dest = memory_object->hval;
            } else if(memory_object->type == MemoryObject::TYPE_BYTE) {
                dest = memory_object->bval;
            } else if(memory_object->type == MemoryObject::TYPE_UNDEFINED) {
                //TODO system->MarkMemoryAsBytes(selection, 1);
                //dest = memory_object->bval;
                assert(false);
            } else {
                assert(false);
            }

            GlobalMemoryLocation label_address(current_selection);
            label_address.address = dest;
            if(!(dest >= memory_region->GetBaseAddress() && dest < memory_region->GetEndAddress())) {
                // destination is not in this memory region, see if we can find it
                vector<u16> possible_banks;
                system->GetBanksForAddress(label_address, possible_banks); // only uses the address field

                if(possible_banks.size() == 1) {
                    label_address.prg_rom_bank = possible_banks[0];
                    memory_region = system->GetMemoryRegion(label_address);
                } else {
                    assert(false); // popup dialog and wait for selection of which bank to go to
                }
            }

            cout << "Creating memory reference to " << label_address << endl;
            {
                auto target_object = system->GetMemoryObject(label_address);
                if(target_object && target_object->labels.size() == 0) { // create a label at that address if there isn't one yet
                    stringstream ss;
                    ss << "L_" << hex << setfill('0') << uppercase;
                    if(system->CanBank(label_address)) ss << setw(2) << label_address.prg_rom_bank;
                    ss << setw(4) << label_address.address;

                    system->CreateLabel(label_address, ss.str());
                }

                // now apply a OperandAddressOrLabel to the data on this memory object
                auto default_operand_format = memory_object->FormatOperandField();
                auto expr = make_shared<Expression>();
                auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(expr->GetNodeCreator());
                auto name = nc->CreateOperandAddressOrLabel(label_address, 0, default_operand_format);
                expr->Set(name);

                // set the expression for memory object at current_selection. it'll show up immediately
                memory_object->operand_expression = expr;
            }


            break;
        }

        case L'F': // very hacky (F)ollow address button
        {
            // TODO this should be way more complicated, parsing operand types and all
            auto memory_region = system->GetMemoryRegion(current_selection);
            if(memory_region) {
                auto memory_object = memory_region->GetMemoryObject(current_selection);
                if(memory_object) {
                    u16 dest = 0;
                    if(memory_object->type == MemoryObject::TYPE_CODE) {
                        dest = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
                    } else if(memory_object->type == MemoryObject::TYPE_WORD) {
                        dest = memory_object->hval;
                    }

                    GoToAddress(dest);
                }
            }
            break;
        }

        default:
            break;

        }
    }

    if(ImGui::IsKeyDown(ImGuiKey_Tab) && !(io.KeyCtrl || io.KeyShift || io.KeyAlt || io.KeySuper)) {
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
    }
}

void Listing::RenderContent() 
{
    // All access goes through the system
    auto system = current_system.lock();
    if(!system) return;

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
        if(jump_to_selection > 0) {
            u32 listing_item_index = memory_region->GetListingIndexByAddress(current_selection);
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

                // then use the address to grab all the listing items belonging to the address
                //cout << "row = 0x" << row << " current_address.address = 0x" << hex << current_address.address << endl;

                bool selected_row = (current_address.address == current_selection.address);

                // Begin a new row and next column
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Create the hidden selectable item
                {
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    char buf[32];
                    sprintf(buf, "##selectable_row%d", row);
                    if (ImGui::Selectable(buf, selected_row, selectable_flags)) {
                        current_selection = current_address;
                    }
                    ImGui::SameLine();
                }

                listing_item->RenderContent(system, current_address, adjust_columns);

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

}
