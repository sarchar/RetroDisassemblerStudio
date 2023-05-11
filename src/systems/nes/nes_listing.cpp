#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

unsigned long ListingItem::common_inner_table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable;

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_unknown", 1, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("??");
        ImGui::EndTable();
    }
}

void ListingItemBlankLine::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_blank", 1, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::Text("");

        ImGui::EndTable();
    }
}

void ListingItemData::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_code", 5, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200));
        ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);

        ImGui::TableNextColumn();
        // nothing
    
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
            ImGui::Text("%s", memory_object->FormatOperandField(internal_offset).c_str());

            ImGui::TableNextColumn();
            if(auto eolc = memory_object->comments.eol) {
                ImGui::Text("; %s", eolc->c_str()); // TODO multiline
            }
        }

        ImGui::EndTable();
    }
}

void ListingItemCode::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(auto mr = memory_region.lock()) {
        auto memory_object = mr->GetMemoryObject(where);
        auto disassembler = system->GetDisassembler();

        u8 op = memory_object->code.opcode;
        u8 sz = disassembler->GetInstructionSize(op);
        if(ImGui::BeginTable("listing_item_code", 5, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
            ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Mnemonic", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Operand", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("EOLComment", ImGuiTableColumnFlags_WidthFixed);

            ImGui::TableNextRow();
        
            ImGui::TableNextColumn();
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, (ImU32)ImColor(200, 200, 200));
            ImGui::Text("$%02X:0x%04X", where.prg_rom_bank, where.address);

            ImGui::TableNextColumn();
            // nothing
    
            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatInstructionField(disassembler).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatOperandField(0, disassembler).c_str());

            ImGui::TableNextColumn();
            if(auto eolc = memory_object->comments.eol) {
                ImGui::Text("; %s", eolc->c_str()); // TODO multiline
            }
        }

        ImGui::EndTable();
    }
}


void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_label", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("        ");

        ImGui::TableNextColumn();
        ImGui::Text("%s:", label->GetString().c_str());
    
        ImGui::EndTable();
    }
}


}
