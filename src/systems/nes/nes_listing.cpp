#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

unsigned long ListingItem::common_inner_table_flags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_Resizable;

void ListingItemUnknown::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
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

void ListingItemBlankLine::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
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

void ListingItemData::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
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
    
        if(auto memory_object = system->GetMemoryObject(where)) {
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

void ListingItemPreComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_comment2", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();
        
        ImGui::TableNextColumn();
        ImGui::Text("        ");
    
        ImGui::TableNextColumn();
        string precomment;
        system->GetComment(where, MemoryObject::COMMENT_TYPE_PRE, precomment); // TODO multiline
        ImGui::Text("; %s", precomment.c_str());

        ImGui::EndTable();
    }
}

void ListingItemPostComment::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_comment2", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed, 4.0f);
        ImGui::TableSetupColumn("Comment", ImGuiTableColumnFlags_WidthFixed);

        ImGui::TableNextRow();
        
        ImGui::TableNextColumn();
        ImGui::Text("        ");

        ImGui::TableNextColumn();
        string postcomment;
        system->GetComment(where, MemoryObject::COMMENT_TYPE_POST, postcomment); // TODO multiline
        ImGui::Text("; %s", postcomment.c_str());

        ImGui::EndTable();
    }
}



void ListingItemCode::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(auto memory_object = system->GetMemoryObject(where)) {
        auto disassembler = system->GetDisassembler();

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

            ImGui::TableNextColumn(); // spacing
    
            ImGui::TableNextColumn();
            ImGui::Text("%s", memory_object->FormatInstructionField(disassembler).c_str());

            ImGui::TableNextColumn();
            if(editing) {
                //InputText(const char* label, std::string* str, ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr);
                ImGui::InputText("", &line_content);
            } else {
                line_content = memory_object->FormatOperandField(0, disassembler);
                ImGui::Text("%s", line_content.c_str());
            }

            ImGui::TableNextColumn();
            string eolcomment;
            system->GetComment(where, MemoryObject::COMMENT_TYPE_EOL, eolcomment); // TODO multiline
            if(eolcomment.size()) ImGui::Text("; %s", eolcomment.c_str());
        }

        ImGui::EndTable();
    }
}


void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where, u32 flags, bool editing)
{
    ImGuiTableFlags table_flags = ListingItem::common_inner_table_flags;
    if(flags) {
        table_flags &= ~ImGuiTableFlags_NoBordersInBody;
        table_flags |= ImGuiTableFlags_BordersInnerV;
    }

    if(ImGui::BeginTable("listing_item_label", 2, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Spacing0", ImGuiTableColumnFlags_WidthFixed);
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
