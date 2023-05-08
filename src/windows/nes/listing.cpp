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
    // Let's render some instructions
    shared_ptr<NESSystem> system = dynamic_pointer_cast<NESSystem>(MyApp::Instance()->GetCurrentSystem());
    if(!system) return;

    // If no cart is loaded, bail
    auto cartridge = system->GetCartridge();
    if(!cartridge) return;

    auto prg_bank0 = cartridge->GetProgramRomBank(1);

    ImVec2 window_size = ImGui::GetWindowSize();
    float line_height = ImGui::GetTextLineHeightWithSpacing();
    int num_lines = (int)((window_size.y / (float)line_height) + 0.5);
    //cout << "[Listing::RenderContent] line height = " << dec << line_height << " num_lines = " << num_lines << endl;

    //static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY /* | ImGuiTableFlags_RowBg */ /* | ImGuiTableFlags_BordersOuter */
    //                               /* | ImGuiTableFlags_BordersV */ | ImGuiTableFlags_Resizable /* | ImGuiTableFlags_Reorderable */ | ImGuiTableFlags_Hideable | ImGuiTableFlags_NoBordersInBody;

    ImGuiTableFlags outer_table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_NoBordersInBody;

    static int selected_row = 0;

    // We use nested tables so that each row can have its own layout. This will be useful when we can render
    // things like plate comments, labels, etc
    if(ImGui::BeginTable("listing_table", 1, outer_table_flags)) {
        ImGui::TableSetupColumn("RowContent", ImGuiTableColumnFlags_WidthStretch);

        ImGuiListClipper clipper;
        clipper.Begin(0x4010);

        while(clipper.Step()) {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                ImGuiTableFlags common_row_flags = ImGuiTableFlags_NoPadOuterX;

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

                    ImGuiTableFlags label_table_flags = common_row_flags | ImGuiTableFlags_NoBordersInBody;
                    if(ImGui::BeginTable("listing_label", 1, label_table_flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("_reset:");
                        ImGui::EndTable();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                }

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
                ImGuiTableFlags code_row_flags = common_row_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;
                if(ImGui::BeginTable("listing_entry", 3, code_row_flags)) {
                    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Insn", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    u16 address = 0 + row;
                    ImGui::Text("0x%04X", address);

                    if(address <= 0x3FFF) {
                        u8 data = prg_bank0->ReadByte(address);

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
