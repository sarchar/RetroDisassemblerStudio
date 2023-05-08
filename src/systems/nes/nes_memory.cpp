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

MemoryRegion::~MemoryRegion()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
    }

    for(auto& blk : content) {
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.ptr != NULL) {
            delete [] blk->data.ptr;
        }
    }
}

void MemoryRegion::Erase()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
        content_ptrs = NULL;
    }

    content.clear();

    // currently limited to 64KiB regions, as large regions might get out of hand using this content_ptrs method
    assert(region_size < 64 * 1024);

    content_ptrs = new u16[region_size];
    memset(content_ptrs, (u16)-1, region_size);
}

void MemoryRegion::InitializeWithData(u16 offset, u16 count, u8* data)
{
    assert(offset + count <= region_size); // make sure we don't overflow the bank

    // Kill all content blocks and references
    Erase();

    // Set the only content block to be one big array of bytes
    shared_ptr<ContentBlock> blk = make_shared<ContentBlock>();
    blk->type = CONTENT_BLOCK_TYPE_DATA;
    blk->offset = offset;
    blk->data.type = CONTENT_BLOCK_DATA_TYPE_UBYTE;
    blk->data.count = count;

    u32 size = sizeof(u8) * count;
    blk->data.ptr = new u8[size];
    memcpy(blk->data.ptr, data, size);

    // Add the content to the bank
    AddContentBlock(blk);

    cout << "[MemoryRegion::InitializeWithData] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data starting at bank offset 0x" << setw(4) << offset << " base 0x" << base_address << endl;
}

void MemoryRegion::AddContentBlock(shared_ptr<ContentBlock>& content_block)
{
    u16 ref = content.size();
    content.push_back(content_block);

    u32 size = content_block->GetSize();
    for(u32 s = content_block->offset; s < content_block->offset + size; s++) {
        content_ptrs[s] = ref;
    }
}

std::shared_ptr<ContentBlock>& MemoryRegion::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    u16 base = ConvertToOffset(where.address);
    u16 cref = content_ptrs[base];
    return content[cref];
}

shared_ptr<ContentBlock> MemoryRegion::SplitContentBlock(GlobalMemoryLocation const& where)
{
    // Attempt to split the content block at `where`. It has to lie on a data type boundary
    auto content_block = GetContentBlockAt(where);
    if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
        cout << "[MemoryRegion::SplitContentBlock] Unupported trying to split non-data block at " << where << endl;
        return nullptr;
    }

    // Make sure the split is aligned
    u16 split_offset = ConvertToOffset(where.address);
    if((split_offset % content_block->GetDataTypeSize()) != 0) {
        cout << "[MemoryRegion::SplitContentBlock] Illegal split at non-aligned boundary at " << where << " requiring alignment " << content_block->GetDataTypeSize() << endl;
        return nullptr;
    }

    // save the old data pointer
    void* data_ptr = content_block->data.ptr;
    assert(data_ptr != nullptr);
    u32 old_count = content_block->data.count;
    
    // create a new one and copy the beginning data do it
    u32 left_size = split_offset - content_block->offset;
    content_block->data.count = left_size / content_block->GetDataTypeSize();
    content_block->data.ptr = new u8[left_size];
    memcpy(content_block->data.ptr, data_ptr, left_size);

    // the remaining data has to go into a new content block
    shared_ptr<ContentBlock> right_block = make_shared<ContentBlock>();
    right_block->type = CONTENT_BLOCK_TYPE_DATA;
    right_block->offset = split_offset;
    right_block->data.type = content_block->data.type;
    right_block->data.count = old_count - content_block->data.count;

    // allocate the storage for the data and copy over the right side
    u32 right_size = right_block->GetSize();
    right_block->data.ptr = new u8[right_size];
    memcpy(right_block->data.ptr, (u8*)data_ptr + left_size, right_size);

    // free old memory
    delete [] data_ptr;

    // return the new object (this leaves content_ptrs out of date, so you better fix them!)
    return right_block;
}

void MemoryRegion::MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type)
{
    auto content_block = GetContentBlockAt(where);
    if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
        cout << "[MemoryRegion::MarkContentAsData] TODO right now can only split other data blocks" << endl;
        return;
    }

    if(content_block->data.type == new_data_type) {
        cout << "MemoryRegion::MarkContentAsData] Content is already of data type " << new_data_type << endl;
        return;
    }

    // Verify the region fits within this content block
    u16 start_offset = ConvertToOffset(where.address);
    if(byte_count > (content_block->GetSize() - start_offset)) {
        cout << "MemoryRegion::MarkContentAsData] Error trying to mark too much data" << endl;
        return;
    }

    // OK, this content block can have the data split. The first part is to split the bank at the start of the new data
    content_block = SplitContentBlock(where);
    if(!content_block) return;
    assert(start_offset == content_block->offset);

    // The new block needs to be added into the system
    AddContentBlock(content_block);

    // This new block may need to be truncated!
    if(byte_count < content_block->GetSize()) {
        GlobalMemoryLocation end_where = where + byte_count;
        shared_ptr<ContentBlock> next_block = SplitContentBlock(end_where);

        if(!next_block) return;
        assert(ConvertToOffset(end_where.address) == next_block->offset);

        // This final block won't be used, but needs to be added to the system
        AddContentBlock(next_block);

        // And let's just refetch the working block JIC
        content_block = GetContentBlockAt(where);
    }

    // now we can convert the data type and update count
    u32 size = content_block->GetSize();
    content_block->data.type = new_data_type;
    content_block->data.count = size / content_block->GetDataTypeSize();

    cout << "[MemoryRegion::MarkContentAsData] marked area " << endl; // TODO print some hex
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProgramRomBank::ProgramRomBank(PROGRAM_ROM_BANK_LOAD _bank_load, PROGRAM_ROM_BANK_SIZE _bank_size) 
    : MemoryRegion(), bank_load(_bank_load), bank_size(_bank_size) 
{

    switch(bank_load) {
    case PROGRAM_ROM_BANK_LOAD_LOW_16K:
        base_address = 0x8000;
        break;

    case PROGRAM_ROM_BANK_LOAD_HIGH_16K:
        base_address = 0xC000;
        break;

    default:
        assert(false);
        break;
    }

    switch(bank_size) {
    case PROGRAM_ROM_BANK_SIZE_16K:
        region_size = 0x4000;
        break;

    case PROGRAM_ROM_BANK_SIZE_32K:
        region_size = 0x8000;
        break;

    default:
        assert(false);
        break;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CharacterRomBank::CharacterRomBank(CHARACTER_ROM_BANK_LOAD _bank_load, CHARACTER_ROM_BANK_SIZE _bank_size)
    : MemoryRegion(), bank_load(_bank_load), bank_size(_bank_size)
{
    switch(bank_load) {
    case CHARACTER_ROM_BANK_LOAD_LOW:
        base_address = 0x0000;
        break;

    case CHARACTER_ROM_BANK_LOAD_HIGH:
        base_address = 0x1000;
        break;

    default:
        assert(false);
    }

    switch(bank_size) {
    case CHARACTER_ROM_BANK_SIZE_4K:
        region_size = 0x1000;
        break;

    case CHARACTER_ROM_BANK_SIZE_8K:
        region_size = 0x2000;
        break;

    default:
        assert(false);
    }
}

}
