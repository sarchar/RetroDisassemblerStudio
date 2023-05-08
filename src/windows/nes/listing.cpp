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

    shared_ptr<NESSystem> system = dynamic_pointer_cast<NESSystem>(MyApp::Instance()->GetCurrentSystem());
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
    if(IsFocused()) { // Things to do only if the window is receiving focus
        CheckInput();
    }
}

void Listing::CheckInput()
{
    ImGuiIO& io = ImGui::GetIO();

    for(int i = 0; i < io.InputQueueCharacters.Size; i++) { 
        ImWchar c = io.InputQueueCharacters[i]; 

        if(c == L's') {
            cout << "split data" << endl;
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
    shared_ptr<NESSystem> system = dynamic_pointer_cast<NESSystem>(MyApp::Instance()->GetCurrentSystem());
    if(!system) return;

    // Need the program rom bank that is currently in the listing
    u16 segment_base = system->GetSegmentBase(selection);
    u16 segment_size = system->GetSegmentSize(selection);

    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags outer_table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody;

    ImGuiWindowFlags_NoNav;

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("listing_table", 1, outer_table_flags)) {
        ImGui::TableSetupColumn("RowContent", ImGuiTableColumnFlags_WidthStretch);

        ImGuiListClipper clipper;
        clipper.Begin(segment_size + 0x100); // TODO: get the current bank and determine the number of elements
        
        // Force the clipper to include a range that also includes the row we want to jump to
        // we have a buffer of 100 lines so ImGui can calculate row heights
        if(jump_to_selection > 0) clipper.ForceDisplayRangeByIndices(selection.address - segment_base - 25, selection.address - segment_base + 25); // TODO need listing item index

        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                // Use 'row' to pick the row'th listing item from the bank.  
                // It's not exactly the bank base address plus row
                // because some rows don't increment the address (i.e., labels, comments, etc)
                // TODO we need listing item counts in order to do this properly
                GlobalMemoryLocation current_address = selection; // copy bank info
                current_address.address = segment_base + row;
                bool selected_row = (current_address.address == selection.address);

                // Fetch the listing items that belong at this address (labels, comments, data, etc)
                vector<shared_ptr<ListingItem>> items;
                system->GetListingItems(current_address, items);

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

                    switch(listing_item->GetType()) {
                    case ListingItem::LISTING_ITEM_TYPE_UNKNOWN:
                        //assert(false);
                        break;

                    case ListingItem::LISTING_ITEM_TYPE_DATA:
                    {
                        ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
                        if(ImGui::BeginTable("listing_item_data", 3, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
                            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
                            ImGui::TableSetupColumn("DataType", ImGuiTableColumnFlags_WidthFixed);
                            ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthFixed);
                            ImGui::TableNextRow();

                            ImGui::TableNextColumn();
                            ImGui::Text("$%02X:0x%04X", current_address.prg_rom_bank, current_address.address);

                            //u8 data = prg_bank0->ReadByte(address);
                            u8 data = 0xEA;

                            ImGui::TableNextColumn();
                            ImGui::Text(".DB");

                            ImGui::TableNextColumn();
                            ImGui::Text("$%02X", data);

                            ImGui::EndTable();
                        }

                        break;
                    }

                    default:
                        assert(false); // unhandled
                    }
                }
                
                // Only after the row has been rendered and it was the last element in the table,
                // we can use ScrollToItem() to get the item focused in the middle of the view.
                if(current_address.address == selection.address && jump_to_selection > 0) {
                    ImGui::ScrollToItem(ImGuiScrollFlags_AlwaysCenterY);
                    jump_to_selection -= 1;
                }

                /*
                // Create a label at $3FFA
                if(row == 0x3FFA) {
                    // Begin a new row and next column
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    // Create the hidden selectable item for the label
                    {
                        ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                        char buf[32];
                        sprintf(buf, "##selectable_label%d_0", row);
                        if (ImGui::Selectable(buf, row == selected_row, selectable_flags)) {
                            selected_row = row;
                        }
                        ImGui::SameLine();
                    }

                    ImGuiTableFlags label_table_flags = common_inner_table_flags | ImGuiTableFlags_NoBordersInBody;
                    if(ImGui::BeginTable("listing_label", 1, label_table_flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("_reset:");
                        ImGui::EndTable();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                }
                */

                /*
                // Begin a new row and next column
                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                // Create the hidden selectable item
                {
                    ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                    char buf[32];
                    sprintf(buf, "##selectable_row%d", row);
                    if (ImGui::Selectable(buf, row == selected_row, selectable_flags)) {
                        selected_row = row;
                    }
                    ImGui::SameLine();
                }

                // List code
                ImGuiTableFlags code_row_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
                if(ImGui::BeginTable("listing_entry", 3, code_row_flags)) {
                    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Insn", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    u16 address = 0 + row;
                    ImGui::Text("0x%04X", address);

                    if(address <= 0x3FFF) {
                        //u8 data = prg_bank0->ReadByte(address);
                        u8 data = 0xEA;

                        ImGui::TableNextColumn();
                        ImGui::Text(".DB");

                        ImGui::TableNextColumn();
                        ImGui::Text("$%02X", data);
                    } else {
                        ImGui::TableNextColumn();
                        ImGui::Text("<empty>");

                        ImGui::TableNextColumn();
                    }

                    ImGui::EndTable();
                }
                */
            }
        }
        ImGui::EndTable();
    }

#if 0
    shared_ptr<SNESSystem> system = dynamic_pointer_cast<SNESSystem>(MyApp::Instance()->GetCurrentSystem());

    if(!system) {
        ImGui::Text("System not loaded");
        return;
    }

#ifdef USE_IMGUI_MEMORY_EDITOR
    static MemoryEditor mem_edit;
    static u8 memory[16384];
    mem_edit.DrawContents(memory, 16384);
    return;
#endif

    const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

    static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit;
    static char const * const columns[] = { "", "00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "0A", "0B", "0C", "0D", "0E", "0F" };

    auto textWidthOne = ImGui::CalcTextSize("0").x;
    ImVec2 cell_padding(textWidthOne, 0.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, cell_padding);
    if (ImGui::BeginTable("snes_memory", 17, flags))
    {
        // set the column width to exactly the size of the text, which is uniform across the table
        auto textWidthAddr  = ImGui::CalcTextSize("0000").x;
        auto textWidthValue = ImGui::CalcTextSize("00").x;

        ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
        for(int col = 0; col < 17; col++) {
            ImGui::TableSetupColumn(columns[col], ImGuiTableColumnFlags_None, (col == 0) ? textWidthAddr : textWidthValue);
        }
        ImGui::TableHeadersRow();

        // Demonstrate using clipper for large vertical lists
        ImGuiListClipper clipper;
        clipper.Begin(0x2000 / 16); // 0x2000 bytes divided by 16 bytes per row
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                ImGui::TableNextRow();
                u16 address = row * 16;
                char buf[16];
                sprintf(buf, "%04X", address);
                ImGui::TableSetColumnIndex(0);
                ImGui::Text(buf);

                // nab 16 bytes starting at address
                u8 values[16];
                system->GetRAM(values, address, 16);

                for (int column = 1; column < 17; column++)
                {
                    ImGui::TableSetColumnIndex(column);
                    sprintf(buf, "%02X", values[column - 1]);
                    ImGui::Text(buf);
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
#endif
}

}
