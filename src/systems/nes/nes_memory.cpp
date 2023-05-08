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
    for(auto& blk : content) {
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.ptr != NULL) {
            delete [] blk->data.ptr;
        }
    }
}

void MemoryRegion::Erase()
{
    content.clear();

    // currently limited to 64KiB regions, as large regions might get out of hand using this content_ptrs method
    assert(region_size < 64 * 1024);

    content_ptrs = make_shared<weak_ptr<ContentBlock>[]>(region_size);

    total_listing_items = 0;
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
    blk->data.elements_per_line = 1;

    u32 size = sizeof(u8) * count;
    blk->data.ptr = new u8[size];
    memcpy(blk->data.ptr, data, size);

    // Compute the number of ListingItems this particular block will create
    // TODO refactor as this is duplicated elsewhere
    blk->num_listing_items = blk->data.count / blk->data.elements_per_line;
    if(blk->num_listing_items * blk->data.elements_per_line < blk->data.count)  blk->num_listing_items += 1; // non-full lines

    // Add the content to the bank
    InsertContentBlock(content.end(), blk);

    cout << "[MemoryRegion::InitializeWithData] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data starting at bank offset 0x" << setw(4) << offset << " base 0x" << base_address << endl;
}

MemoryRegion::ContentBlockListType::iterator MemoryRegion::InsertContentBlock(MemoryRegion::ContentBlockListType::iterator loc, shared_ptr<ContentBlock>& content_block)
{
    u16 ref = content.size();
    ContentBlockListType::iterator it = content.insert(loc, content_block);

    u32 size = content_block->GetSize();
    for(u32 s = content_block->offset; s < content_block->offset + size; s++) {
        content_ptrs[s] = content_block;
    }

    total_listing_items += content_block->num_listing_items;
    return it;
}

std::shared_ptr<ContentBlock> MemoryRegion::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    u16 base = ConvertToOffset(where.address);
    return content_ptrs[base].lock();
}

shared_ptr<ContentBlock> MemoryRegion::SplitContentBlock(GlobalMemoryLocation const& where)
{
    // Attempt to split the content block at `where`. It has to lie on a data type boundary
    auto content_block = GetContentBlockAt(where);
    cout << "Splitting block 0x" << hex << content_block->offset << endl;
    if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
        cout << "[MemoryRegion::SplitContentBlock] Unupported trying to split non-data block at " << where << endl;
        return nullptr;
    }

    // Make sure the split is aligned
    u16 split_offset = ConvertToOffset(where.address) - content_block->offset;
    cout << "Split offset = 0x" << hex << split_offset << endl;
    if((split_offset % content_block->GetDataTypeSize()) != 0) {
        cout << "[MemoryRegion::SplitContentBlock] Illegal split at non-aligned boundary at " << where << " requiring alignment " << content_block->GetDataTypeSize() << endl;
        return nullptr;
    }

    // save the old data pointer
    void* data_ptr = content_block->data.ptr;
    assert(data_ptr != nullptr);
    u32 old_count = content_block->data.count;
    u32 old_num_listing_items = content_block->num_listing_items;
    
    // create a new one and copy the beginning data do it
    u32 left_size = split_offset;
    cout << "Left size = 0x" << left_size << endl;
    content_block->data.count = left_size / content_block->GetDataTypeSize();
    content_block->data.ptr = new u8[left_size];
    memcpy(content_block->data.ptr, data_ptr, left_size);

    // update the number of listing items in this block
    content_block->num_listing_items = content_block->data.count / content_block->data.elements_per_line;
    if(content_block->num_listing_items * content_block->data.elements_per_line < content_block->data.count)  content_block->num_listing_items += 1; // non-full lines

    // update the global # of listing items
    total_listing_items -= (old_num_listing_items - content_block->num_listing_items);

    // the remaining data has to go into a new content block
    shared_ptr<ContentBlock> right_block = make_shared<ContentBlock>();
    right_block->type = CONTENT_BLOCK_TYPE_DATA;
    right_block->offset = content_block->offset + split_offset;
    right_block->data.type = content_block->data.type;
    right_block->data.count = old_count - content_block->data.count;
    right_block->data.elements_per_line = 1;

    // count the number of listing items in this block
    // but don't add them to the global yet, it will get added later in InsertContentBlock
    right_block->num_listing_items = right_block->data.count / right_block->data.elements_per_line;
    if(right_block->num_listing_items * right_block->data.elements_per_line < right_block->data.count)  right_block->num_listing_items += 1; // non-full lines

    // allocate the storage for the data and copy over the right side
    u32 right_size = right_block->GetSize();
    cout << "Right size = 0x" << right_size << endl;
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
    u16 start_offset = ConvertToOffset(where.address) - content_block->offset;
    if(byte_count > (content_block->GetSize() - start_offset)) {
        cout << "MemoryRegion::MarkContentAsData] Error trying to mark too much data" << endl;
        return;
    }

    // OK, this content block can have the data split. The first part is to split the bank at the start of the new data
    cout << "Before split:" << endl;
    PrintContentBlocks();

    ContentBlockListType::iterator it = find(content.begin(), content.end(), content_block); // look for the block with our data
    assert(it != content.end());

    // Only split the left half if we're in the middle of the block
    if(start_offset > 0) {
        auto new_block = SplitContentBlock(where);
        if(!new_block) return;
        assert(start_offset == new_block->offset);

        // The new block needs to be added into the system right after the last block
        it = InsertContentBlock(++it, new_block); // add the new block right after it

        // and now work with new_block
        content_block = new_block;
    }

    // This new block may need to be truncated!
    if(byte_count < content_block->GetSize()) {
        GlobalMemoryLocation end_where = where + byte_count;
        shared_ptr<ContentBlock> next_block = SplitContentBlock(end_where);
        if(!next_block) return;

        assert(ConvertToOffset(end_where.address) - next_block->offset == 0);

        // This final block won't be changed, but needs to be added to the system right after content_block
        InsertContentBlock(++it, next_block);

        // And let's just refetch the working block JIC
        content_block = GetContentBlockAt(where);
    }

    // now we can convert the data type and update count
    u32 size = content_block->GetSize();
    content_block->data.type = new_data_type;
    content_block->data.count = size / content_block->GetDataTypeSize();

    // and then recalculate the number of listing items
    int old_num_listing_items = (int)content_block->num_listing_items;
    content_block->num_listing_items = content_block->data.count / content_block->data.elements_per_line;
    if(content_block->num_listing_items * content_block->data.elements_per_line < content_block->data.count)  content_block->num_listing_items += 1; // non-full lines
    total_listing_items -= (old_num_listing_items - content_block->num_listing_items);

    cout << "After split:" << endl;
    PrintContentBlocks();
    cout << "[MemoryRegion::MarkContentAsData] marked area " << endl; // TODO print some hex
}

u32 MemoryRegion::GetListingIndexByAddress(GlobalMemoryLocation const& where)
{
    //TODO
    auto content_block = GetContentBlockAt(where);
    return ConvertToOffset(where.address) - content_block->offset;
}

u32 MemoryRegion::GetAddressForListingItemIndex(u32 listing_item_index)
{
    // this iterative approach is inefficient, and depends on the content blocks vector to be sorted
    // but it'll do for now and should be optimized one day. plus, this listing items format is probably
    // not even close to its final form
    for(auto& blk : content) {
        if(blk->type != CONTENT_BLOCK_TYPE_DATA) continue;

        // Check if our listing item is in this content block
        if(listing_item_index >= blk->num_listing_items) {
            listing_item_index -= blk->num_listing_items;
            continue;
        }

        // OK it's in this block, figure out the address
        return base_address + blk->offset + listing_item_index * blk->GetDataTypeSize();
    }

    return 0;
}

void MemoryRegion::PrintContentBlocks()
{
    int i = 0;
    for(auto& blk : content) {
        u32 size = blk->GetSize();
        cout << "Block " << i << " offset = 0x" << hex << blk->offset << " type " << blk->type 
             << " datatype " << blk->data.type 
             << " size 0x" << size << endl;
    }
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
