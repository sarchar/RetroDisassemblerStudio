#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_content.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

std::string ContentBlock::FormatInstructionField() 
{
    switch(type) {
    case CONTENT_BLOCK_TYPE_DATA:
        switch(data.type) {
        case CONTENT_BLOCK_DATA_TYPE_UBYTE:
            return ".DB";
        case CONTENT_BLOCK_DATA_TYPE_UWORD:
            return ".DW";
        default:
            assert(false);
            return "";
        }
        break;

    default:
        assert(false);
        return "";
    }
}

std::string ContentBlock::FormatDataElement(u16 n) 
{
    std::stringstream ss;

    assert(type == CONTENT_BLOCK_TYPE_DATA);
    switch(data.type) {
    case CONTENT_BLOCK_DATA_TYPE_UBYTE:
        ss << "$" << std::hex << std::setfill('0') << std::setw(2) << (u16)((u8*)data.ptr)[n];
        break;

    case CONTENT_BLOCK_DATA_TYPE_UWORD:
    {
        u8* p = (u8*)&((u16*)data.ptr)[n];
        u16  v = p[0] | (p[1] << 8);
        ss << "$" << std::hex << std::setfill('0') << std::setw(4) << v;
        break;
    }

    default:
        break;
    }

    return ss.str();
}

void ListingItemData::RenderContent(shared_ptr<System>& system)
{
    ImGuiTableFlags common_inner_table_flags = ImGuiTableFlags_NoPadOuterX;
    ImGuiTableFlags table_flags = common_inner_table_flags | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable;

    if(ImGui::BeginTable("listing_item_data", 3, table_flags)) { // using the same name for each data TYPE allows column sizes to line up
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("DataType", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableNextRow();
    
        ImGui::TableNextColumn();
        ImGui::Text("$%02X:0x%04X", global_memory_location.prg_rom_bank, global_memory_location.address);
    
        auto content_block = system->GetContentBlockAt(global_memory_location);
        assert(content_block->type == CONTENT_BLOCK_TYPE_DATA);

        //u8 data = prg_bank0->ReadByte(address);
        //u8 data = 0xEA;
    
        ImGui::TableNextColumn();
        ImGui::Text(content_block->FormatInstructionField().c_str());
    
        ImGui::TableNextColumn();
        // TODO this 0xC000 subtraction should come from the bank knowing where it's loaded
        // right now I only have the content block, which will need to know what bank it's in
        // that can only happen once I abstract out memory regions and create program rom/character rom and 
        // various memory banks derived from memory regions.
        // Something like: content_block->GetContainingMemoryRegion()->ConvertToOffset(global address)
        u16 n = (global_memory_location.address - 0xC000 - content_block->offset) / content_block->GetDataTypeSize();
        ImGui::Text(content_block->FormatDataElement(n).c_str());
    
        ImGui::EndTable();
    }
}

void ListingItemUnknown::RenderContent(shared_ptr<System>& system)
{
}

}
