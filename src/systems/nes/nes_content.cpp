#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_content.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

//! std::string ContentBlock::FormatInstructionField() 
//! {
//!     switch(type) {
//!     case CONTENT_BLOCK_TYPE_DATA:
//!         switch(data.type) {
//!         case CONTENT_BLOCK_DATA_TYPE_UBYTE:
//!             return ".DB";
//!         case CONTENT_BLOCK_DATA_TYPE_UWORD:
//!             return ".DW";
//!         default:
//!             assert(false);
//!             return "";
//!         }
//!         break;
//! 
//!     default:
//!         assert(false);
//!         return "";
//!     }
//! }
//! 
//! std::string ContentBlock::FormatDataElement(u16 n) 
//! {
//!     std::stringstream ss;
//! 
//!     assert(type == CONTENT_BLOCK_TYPE_DATA);
//!     switch(data.type) {
//!     case CONTENT_BLOCK_DATA_TYPE_UBYTE:
//!         ss << "$" << std::hex << std::setfill('0') << std::setw(2) << (u16)((u8*)data.ptr)[n];
//!         break;
//! 
//!     case CONTENT_BLOCK_DATA_TYPE_UWORD:
//!     {
//!         u8* p = (u8*)&((u16*)data.ptr)[n];
//!         u16  v = p[0] | (p[1] << 8);
//!         ss << "$" << std::hex << std::setfill('0') << std::setw(4) << v;
//!         break;
//!     }
//! 
//!     default:
//!         break;
//!     }
//! 
//!     return ss.str();
//! }

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

void ListingItemLabel::RenderContent(shared_ptr<System>& system, GlobalMemoryLocation const& where)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(ImGui::BeginTable("listing_item_label", 3, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("%s:", label_name.c_str());
    
        ImGui::EndTable();
    }
}


}
