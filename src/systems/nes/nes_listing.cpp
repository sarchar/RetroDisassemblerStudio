#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(ImGui::BeginTable("listing_item_unknown", 1, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("??");
        ImGui::EndTable();
    }
}

void ListingItemData::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(ImGui::BeginTable("listing_item_data", 3, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("DataType", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);
    
        if(auto mr = memory_region.lock()) {
            auto memory_object = mr->GetMemoryObject(where);

            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatInstructionField().c_str());

            // The internal_offset value will be used to index into the middle of data arrays, so that
            // multiple data listing items can show something like:
            // 
            // .DB $01, $02, $03,
            //     $04, $05, $06
            //     $07
            //
            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatDataField(internal_offset).c_str());
        }

        ImGui::EndTable();
    }
}

void ListingItemCode::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(auto mr = memory_region.lock()) {
        auto memory_object = mr->GetMemoryObject(where);
        auto disassembler = system->GetDisassembler();

        u8 op = memory_object->code.opcode;
        u8 sz = disassembler->GetInstructionSize(op);
        if(ImGui::BeginTable("listing_item_code", 3, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);

            ImGui::TableNextRow();
        
            ImGui::TableNextColumn();
            ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);
    
            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatInstructionField(disassembler).c_str());

            ImGui::TableNextColumn();
            if(sz == 1) {
                ImGui::Text("");
            } else {
                ImGui::Text("%s", memory_object->FormatDataField(0, disassembler).c_str());
            }
        }

        ImGui::EndTable();
    }
}


void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(ImGui::BeginTable("listing_item_label", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("        ");

        ImGui::TableNextColumn();
        ImGui::Text("%s:", label_name.c_str());
    
        ImGui::EndTable();
    }
}


}
