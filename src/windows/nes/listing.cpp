// TODO in the future will need a way to make windows only available to the currently loaded system
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

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
    : BaseWindow("NES::Listing"),
      jump_to_selection(0)
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
//    if(IsFocused()) { // Things to do only if the window is receiving focus
//        CheckInput();
//    }
}

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    for(int i = 0; i < io.InputQueueCharacters.Size; i++) { 
        ImWchar c = io.InputQueueCharacters[i]; 

        if(c == L'w') {
            cout << "input" << endl;
            // mark data as a word
            shared_ptr<System> system = dynamic_pointer_cast<System>(MyApp::Instance()->GetCurrentSystem());
            if(system) {
                system->MarkContentAsData(selection, 2, CONTENT_BLOCK_DATA_TYPE_UWORD);
            }
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

    // Need the program rom bank that is currently in the listing
    auto memory_region = system->GetMemoryRegion(selection);

    ImGuiTableFlags outer_table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody;

    ImGuiWindowFlags_NoNav;

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
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd;) {
                // Convert the listing index into the corresponding address
                // then use the address to grab all the listing items belonging to the address
                GlobalMemoryLocation current_address(selection); // start with a copy since we're in the same memory region
                current_address.address = memory_region->GetAddressForListingItemIndex(row);
                //cout << "row = 0x" << row << " current_address.address = 0x" << hex << current_address.address << endl;

                bool selected_row = (current_address.address == selection.address);

                // Fetch the listing items that belong at this address (labels, comments, data, etc)
                vector<shared_ptr<ListingItem>> items;
                system->GetListingItems(current_address, items);
                assert(items.size() > 0);

                // Draw the items
                for(auto& listing_item : items) {
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

                    listing_item->RenderContent(system);

                    // keep track of how many rows we add
                    if(++row >= clipper.DisplayEnd) break;
                }
                
                // Only after the row has been rendered and it was the last element in the table,
                // we can use ScrollToItem() to get the item focused in the middle of the view.
                if(current_address.address == selection.address && jump_to_selection > 0) {
                    ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
                    jump_to_selection -= 1;
                }
            }
        }
        ImGui::EndTable();
    }

    if(IsFocused()) { // Things to do only if the window is receiving focus
        CheckInput();
    }

}

}
