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
    SetNav(false); // dsisable navigation

    shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
    if(system) {
        system->GetEntryPoint(&selection);
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE; // TODO this is stupid, I wish I could scroll within one or two frames (given we have to calculate the row sizes at least once)
    }

    cout << "[NES::Listing] Program entry point at " << selection << endl;
}

Listing::~Listing()
{
}

void Listing::UpdateContent(double deltaTime) 
{
}

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    if(ImGui::IsKeyPressed(ImGuiKey_MouseX1) && location_history.size()) { // back pressed
        selection = location_history.back();
        location_history.pop_back();
        jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
    }

    for(int i = 0; i < io.InputQueueCharacters.Size; i++) { 
        ImWchar c = io.InputQueueCharacters[i]; 

        shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
        // TODO really should be using signals and emit to broadcast these messages
        switch(c) {
        case L'w':
            // mark data as a word
            if(system) system->MarkMemoryAsWords(selection, 2);
            break;

        case L'd':
            // start disassembly
            if(system) {
                system->BeginDisassembly(selection);
                show_disassembling_popup = true;
            }
            break;

        case L'l':
            // create a new label at the current address
            create_new_label = true;
            break;

        case L'j':
            selection = selection + 1;
            break;

        case L'k':
            selection = selection + -1;
            break;

        case L'G': // very hacky (G)o to address button
        {
            // TODO this should be way more complicated, parsing operand types and all
            auto memory_region = system->GetMemoryRegion(selection);
            auto memory_object = memory_region->GetMemoryObject(selection);
            u16 dest = 0;
            if(memory_object->type == MemoryObject::TYPE_CODE) {
                dest = (u16)memory_object->code.operands[0] | ((u16)memory_object->code.operands[1] << 8);
            } else if(memory_object->type == MemoryObject::TYPE_WORD) {
                dest = memory_object->hval;
            }

            if(dest >= memory_region->GetBaseAddress() && dest < memory_region->GetEndAddress()) {
                location_history.push_back(selection); // save the current location to the history
                
                selection.address = dest;
                jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
            } else {
                // destination is not in this memory region, see if we can find it
                GlobalMemoryLocation guessed_address(selection);
                guessed_address.address = dest;

                vector<u16> possible_banks;
                system->GetBanksForAddress(guessed_address, possible_banks);

                if(possible_banks.size() == 1) {
                    guessed_address.prg_rom_bank = possible_banks[0];
                    memory_region = system->GetMemoryRegion(guessed_address);
                    if(memory_region) {
                        location_history.push_back(selection); // save the current location to the history
                        selection = guessed_address;
                        jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
                    }
                } else {
                    assert(false); // popup dialog and wait for selection of which bank to go to
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

void Listing::PreRenderContent()
{
    // Initialize this window on a dock
    if(MyApp::Instance()->HasDockBuilder()) {
        ImGuiID dock_node_id = (ImGuiID)MyApp::Instance()->GetDockBuilderRootID();
        ImGui::SetNextWindowDockID(dock_node_id, ImGuiCond_Appearing);
    }
}

void Listing::RenderContent() 
{
    // All access goes through the system
    shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
    if(!system) return;

    {
        bool need_pop = false;
        if(adjust_columns) {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(255, 0, 0));
            need_pop = true;
        }

        if(ImGui::SmallButton("R")) adjust_columns = !adjust_columns;

        if(need_pop) ImGui::PopStyleColor(1);

        ImGui::Separator();
    }

    // Need the program rom bank that is currently in the listing
    auto memory_region = system->GetMemoryRegion(selection);

    ImGuiTableFlags outer_table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody;

    ImGuiWindowFlags_NoNav;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(-1, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(-1, 0));

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("listing_table", 1, outer_table_flags)) {
        ImGui::TableSetupColumn("RowContent", ImGuiTableColumnFlags_WidthStretch);

        ImGuiListClipper clipper;
        u32 total_listing_items = memory_region->GetTotalListingItems();
        //cout << "total_listing_items = 0x" << hex << total_listing_items << endl;
        clipper.Begin(total_listing_items); // TODO: get the current bank and determine the number of elements
        
        // Force the clipper to include a range that also includes the row we want to jump to
        // we have a buffer of 100 lines so ImGui can calculate row heights
        if(jump_to_selection > 0) {
            u32 listing_item_index = memory_region->GetListingIndexByAddress(selection);
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
                GlobalMemoryLocation current_address(selection); // start with a copy since we're in the same memory region
                current_address.address = listing_item_iterator->GetCurrentAddress();

                // then use the address to grab all the listing items belonging to the address
                //cout << "row = 0x" << row << " current_address.address = 0x" << hex << current_address.address << endl;

                bool selected_row = (current_address.address == selection.address);

                // Begin a new row and next column
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Create the hidden selectable item
                {
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    char buf[32];
                    sprintf(buf, "##selectable_row%d", row);
                    if (ImGui::Selectable(buf, selected_row, selectable_flags)) {
                        selection = current_address;
                    }
                    ImGui::SameLine();
                }

                listing_item->RenderContent(system, current_address, adjust_columns);

                // Only after the row has been rendered and it was the last element in the table,
                // we can use ScrollToItem() to get the item focused in the middle of the view.
                if(jump_to_selection > 0 && current_address.address == selection.address && !did_scroll) {
                    ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
                    jump_to_selection -= 1;
                    did_scroll = true;
                }
            }
        }
        ImGui::EndTable();
    }

    ImGui::PopStyleVar(2);

    // only scan input if the window is receiving focus
    if(IsFocused() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        CheckInput();
    }

    // Render popups
    NewLabelPopup();
    DisassemblyPopup();
}

void Listing::NewLabelPopup() 
{
    static char title[] = "Create new label...";

    if(!ImGui::IsPopupOpen(title) && create_new_label) {
        create_new_label = false;
        new_label_buffer[0] = '\0';

        ImGui::OpenPopup(title);
    }

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool ret = false;
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        // focus on the text input if nothing else is selected
        if(!ImGui::IsAnyItemActive()) ImGui::SetKeyboardFocusHere();

        ImVec2 button_size(ImGui::GetFontSize() * 7.0f, 0.0f);
        if(ImGui::InputText("Label", new_label_buffer, sizeof(new_label_buffer), ImGuiInputTextFlags_EnterReturnsTrue)
                || ImGui::Button("OK", button_size)) {
            ret = true;
            ImGui::CloseCurrentPopup(); 
        }

        if(ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if(ret) {
        // create the label
        string lstr(new_label_buffer);

        shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
        if(!system) return;
        system->CreateLabel(selection, lstr);
    }
}

void Listing::DisassemblyPopup()
{
    static char title[] = "Disassembling...";

    if(!ImGui::IsPopupOpen(title) && show_disassembling_popup) {
        show_disassembling_popup = false;
        ImGui::OpenPopup(title);

        shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
        disassembly_thread = make_unique<std::thread>(std::bind(&System::DisassemblyThread, system));
        cout << "[Listing::DisassemblyPopup] started disassembly thread" << endl;
    }

    // center this window
    ImVec2 center = ImGui::GetMainViewport()->GetCenter(); // TODO center on the current Listing window?
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool ret = false;
    if (ImGui::BeginPopupModal(title, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::Text("Disassembling...");

        shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
        if(!system || !system->IsDisassembling()) {
            disassembly_thread->join();
            disassembly_thread = nullptr;
            cout << "[Listing::DisassemblyPopup] disassembly thread exited" << endl;
            jump_to_selection = JUMP_TO_SELECTION_START_VALUE;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}
