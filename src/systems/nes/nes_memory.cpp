#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

MemoryRegion::MemoryRegion(shared_ptr<System>& _parent_system) 
{ 
    // create a week reference to avoid cyclical refs
    parent_system = _parent_system;
}

MemoryRegion::~MemoryRegion()
{
}

void MemoryRegion::Erase()
{
    object_refs.clear();
    object_tree_root = nullptr;
}

// Recalculate all the listing_item_count in the memory object tree
void MemoryRegion::_RecalculateListingItemCounts(shared_ptr<MemoryObjectTreeNode>& tree_node)
{
    if(tree_node->is_object) {
        tree_node->listing_item_count = tree_node->obj->listing_items.size();
    } else {
        _RecalculateListingItemCounts(tree_node->left);
        _RecalculateListingItemCounts(tree_node->right);
        tree_node->listing_item_count = tree_node->left->listing_item_count + tree_node->right->listing_item_count;
    }
}

void MemoryRegion::RecreateListingItems()
{
    u32 region_offset = 0;
    while(region_offset < region_size) {
        shared_ptr<MemoryObject> obj = object_refs[region_offset];
        RecreateListingItemsForMemoryObject(obj, region_offset);

        // skip memory that points to the same object
        region_offset += 1;
        while(region_offset < region_size && object_refs[region_offset] == obj) ++region_offset;
    }
}

void MemoryRegion::RecalculateListingItemCounts()
{
    _RecalculateListingItemCounts(object_tree_root);
}

void MemoryRegion::RecreateListingItemsForMemoryObject(shared_ptr<MemoryObject>& obj, u32 region_offset)
{
    // NOTE: do NOT save region_offset in the memory object! It'll be wrong when objects in object_refs move around

    // For now, objects only have 1 listing item: the data itself
    // but in the future, we need to count up labels, comments, etc
    obj->listing_items.clear();

    for(auto& label : obj->labels) {
        obj->listing_items.push_back(make_shared<ListingItemLabel>(label));
        //    stringstream ss;
        //    ss << "L_" << hex << setw(4) << uppercase << setfill('0') << region_offset;
        //    obj->listing_items.push_back(make_shared<ListingItemLabel>(ss.str()));
        //}
    }

    obj->listing_items.push_back(make_shared<ListingItemData>(shared_from_this(), 0));

    //!if(i == 0x3FFC) {
    //!    obj->listing_items.push_back(make_shared<ListingItemLabel>("    ; this is a comment"));
    //!}
}

void MemoryRegion::_InitializeFromData(shared_ptr<MemoryObjectTreeNode>& tree_node, u32 region_offset, u8* data, int count)
{
    // stop the iteration when there's one byte left
    if(count == 1) {
        tree_node->is_object = true;

        // create the object
        shared_ptr<MemoryObject> obj = make_shared<MemoryObject>();
        obj->parent = tree_node;

        // set the data
        obj->type = MemoryObject::TYPE_UNDEFINED;
        obj->bval = *data;

        // set the element in the node
        tree_node->obj = obj;

        // and create the memory address reference to the object
        object_refs[region_offset] = obj;
    } else {
        // initialize the tree by splitting the data into left and right halves
        tree_node->left  = make_shared<MemoryObjectTreeNode>(tree_node);
        tree_node->right = make_shared<MemoryObjectTreeNode>(tree_node);
        _InitializeFromData(tree_node->left , region_offset            , &data[0]        , count / 2);
        _InitializeFromData(tree_node->right, region_offset + count / 2, &data[count / 2], count / 2);
    }
}

void MemoryRegion::InitializeFromData(u8* data, int count)
{
    assert(count == region_size); // can only initialize the memory region tree with an exact number of bytes

    // Kill all content blocks and references
    Erase();

    // the refs list is a object lookup by address map, and will always be the size of the memory region
    object_refs.resize(count);

    // We need a root for the tree first and foremost
    object_tree_root = make_shared<MemoryObjectTreeNode>(nullptr);

    // initialize the tree by splitting the data into left and right halves
    assert((count % 2) == 0);
    object_tree_root->left  = make_shared<MemoryObjectTreeNode>(object_tree_root);
    object_tree_root->right = make_shared<MemoryObjectTreeNode>(object_tree_root);
    _InitializeFromData(object_tree_root->left , 0        , &data[0]        , count / 2);
    _InitializeFromData(object_tree_root->right, count / 2, &data[count / 2], count / 2);

    // first pass create listing items
    RecreateListingItems();
    RecalculateListingItemCounts();

    cout << "[MemoryRegion::InitializeWithData] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data for memory base 0x" << setw(4) << base_address << endl;
}

//! shared_ptr<ContentBlock> MemoryRegion::SplitContentBlock(GlobalMemoryLocation const& where)
//! {
//!     // Attempt to split the content block at `where`. It has to lie on a data type boundary
//!     auto content_block = GetContentBlockAt(where);
//!     cout << "Splitting block 0x" << hex << content_block->offset << endl;
//!     if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
//!         cout << "[MemoryRegion::SplitContentBlock] Unupported trying to split non-data block at " << where << endl;
//!         return nullptr;
//!     }
//! 
//!     // Make sure the split is aligned
//!     u16 split_offset = ConvertToRegionOffset(where.address) - content_block->offset;
//!     cout << "Split offset = 0x" << hex << split_offset << endl;
//!     if((split_offset % content_block->GetDataTypeSize()) != 0) {
//!         cout << "[MemoryRegion::SplitContentBlock] Illegal split at non-aligned boundary at " << where << " requiring alignment " << content_block->GetDataTypeSize() << endl;
//!         return nullptr;
//!     }
//! 
//!     // save the old data pointer
//!     void* data_ptr = content_block->data.ptr;
//!     assert(data_ptr != nullptr);
//!     u32 old_count = content_block->data.count;
//!     u32 old_num_listing_items = content_block->num_listing_items;
//!     
//!     // create a new one and copy the beginning data do it
//!     u32 left_size = split_offset;
//!     cout << "Left size = 0x" << left_size << endl;
//!     content_block->data.count = left_size / content_block->GetDataTypeSize();
//!     content_block->data.ptr = new u8[left_size];
//!     memcpy(content_block->data.ptr, data_ptr, left_size);
//! 
//!     // update the number of listing items in this block
//!     content_block->num_listing_items = content_block->data.count / content_block->data.elements_per_line;
//!     if(content_block->num_listing_items * content_block->data.elements_per_line < content_block->data.count)  content_block->num_listing_items += 1; // non-full lines
//! 
//!     // update the global # of listing items
//!     total_listing_items -= (old_num_listing_items - content_block->num_listing_items);
//! 
//!     // the remaining data has to go into a new content block
//!     shared_ptr<ContentBlock> right_block = make_shared<ContentBlock>();
//!     right_block->type = CONTENT_BLOCK_TYPE_DATA;
//!     right_block->offset = content_block->offset + split_offset;
//!     right_block->data.type = content_block->data.type;
//!     right_block->data.count = old_count - content_block->data.count;
//!     right_block->data.elements_per_line = 1;
//! 
//!     // count the number of listing items in this block
//!     // but don't add them to the global yet, it will get added later in InsertContentBlock
//!     right_block->num_listing_items = right_block->data.count / right_block->data.elements_per_line;
//!     if(right_block->num_listing_items * right_block->data.elements_per_line < right_block->data.count)  right_block->num_listing_items += 1; // non-full lines
//! 
//!     // allocate the storage for the data and copy over the right side
//!     u32 right_size = right_block->GetSize();
//!     cout << "Right size = 0x" << right_size << endl;
//!     right_block->data.ptr = new u8[right_size];
//!     memcpy(right_block->data.ptr, (u8*)data_ptr + left_size, right_size);
//! 
//!     // free old memory
//!     delete [] data_ptr;
//! 
//!     // return the new object (this leaves content_ptrs out of date, so you better fix them!)
//!     return right_block;
//! }
//! 
//! void MemoryRegion::MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type)
//! {
//!     auto content_block = GetContentBlockAt(where);
//!     if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
//!         cout << "[MemoryRegion::MarkContentAsData] TODO right now can only split other data blocks" << endl;
//!         return;
//!     }
//! 
//!     if(content_block->data.type == new_data_type) {
//!         cout << "MemoryRegion::MarkContentAsData] Content is already of data type " << new_data_type << endl;
//!         return;
//!     }
//! 
//!     // Verify the region fits within this content block
//!     u16 start_offset = ConvertToRegionOffset(where.address) - content_block->offset;
//!     if(byte_count > (content_block->GetSize() - start_offset)) {
//!         cout << "MemoryRegion::MarkContentAsData] Error trying to mark too much data" << endl;
//!         return;
//!     }
//! 
//!     // OK, this content block can have the data split. The first part is to split the bank at the start of the new data
//!     cout << "Before split:" << endl;
//!     PrintContentBlocks();
//! 
//!     ContentBlockListType::iterator it = find(content.begin(), content.end(), content_block); // look for the block with our data
//!     assert(it != content.end());
//! 
//!     // Only split the left half if we're in the middle of the block
//!     if(start_offset > 0) {
//!         auto new_block = SplitContentBlock(where);
//!         if(!new_block) return;
//!         assert(start_offset == new_block->offset);
//! 
//!         // The new block needs to be added into the system right after the last block
//!         it = InsertContentBlock(++it, new_block); // add the new block right after it
//! 
//!         // and now work with new_block
//!         content_block = new_block;
//!     }
//! 
//!     // This new block may need to be truncated!
//!     if(byte_count < content_block->GetSize()) {
//!         GlobalMemoryLocation end_where = where + byte_count;
//!         shared_ptr<ContentBlock> next_block = SplitContentBlock(end_where);
//!         if(!next_block) return;
//! 
//!         assert(ConvertToRegionOffset(end_where.address) - next_block->offset == 0);
//! 
//!         // This final block won't be changed, but needs to be added to the system right after content_block
//!         InsertContentBlock(++it, next_block);
//! 
//!         // And let's just refetch the working block JIC
//!         content_block = GetContentBlockAt(where);
//!     }
//! 
//!     // now we can convert the data type and update count
//!     u32 size = content_block->GetSize();
//!     content_block->data.type = new_data_type;
//!     content_block->data.count = size / content_block->GetDataTypeSize();
//! 
//!     // and then recalculate the number of listing items
//!     int old_num_listing_items = (int)content_block->num_listing_items;
//!     content_block->num_listing_items = content_block->data.count / content_block->data.elements_per_line;
//!     if(content_block->num_listing_items * content_block->data.elements_per_line < content_block->data.count)  content_block->num_listing_items += 1; // non-full lines
//!     total_listing_items -= (old_num_listing_items - content_block->num_listing_items);
//! 
//!     cout << "After split:" << endl;
//!     PrintContentBlocks();
//!     cout << "[MemoryRegion::MarkContentAsData] marked area " << endl; // TODO print some hex
//! }

shared_ptr<MemoryObject> MemoryRegion::GetMemoryObject(GlobalMemoryLocation const& where)
{
    return object_refs[ConvertToRegionOffset(where.address)];
}

u32 MemoryRegion::GetListingIndexByAddress(GlobalMemoryLocation const& where)
{
    // Get the MemoryObject at where
    u32 region_offset = ConvertToRegionOffset(where.address);
    auto obj = object_refs[region_offset];

    // Get the first listing at the current address (start at 0)
    u32 listing_item_index = 0;

    // start with the MemoryObjectTreeNode 
    auto last_node = obj->parent.lock();
    assert(last_node->is_object);
    auto current_node = last_node->parent.lock();
    assert(current_node); // all is_object nodes will have a parent

    // Simply add all the left nodes until we reach the root of the tree
    while(current_node) {
        if(current_node->left && current_node->left != last_node) { // we didn't come from the left (and there is a left)
            listing_item_index += current_node->left->listing_item_count;
        }

        last_node = current_node;
        current_node = current_node->parent.lock();
    }

    return listing_item_index;
}

void MemoryRegion::UpdateMemoryObject(GlobalMemoryLocation const& where)
{
    u32 region_offset = ConvertToRegionOffset(where.address);
    auto memory_object = object_refs[region_offset];

    // recreate the listing items for this one object
    RecreateListingItemsForMemoryObject(memory_object, region_offset);

    // propagate up the tree the changes
    auto current_node = memory_object->parent.lock();
    current_node->listing_item_count = memory_object->listing_items.size(); // update the is_object node
    current_node = current_node->parent.lock();
    while(current_node) {
        current_node->listing_item_count = current_node->left->listing_item_count + current_node->right->listing_item_count;
        current_node = current_node->parent.lock();
    }
}

void MemoryRegion::CreateLabel(GlobalMemoryLocation const& where, string const& label)
{
    u32 region_offset = ConvertToRegionOffset(where.address);
    auto memory_object = object_refs[region_offset];

    // add the label
    memory_object->labels.push_back(label);

    // update the object
    UpdateMemoryObject(where);
}

// TODO use a binary search through the object_refs. will need another function that
// determines the listing_item_index of the first listing item within a memoryobject
//! u32 MemoryRegion::FindRegionOffsetForListingItem(int listing_item_index)
//! {
//! }

shared_ptr<MemoryObjectTreeNode::iterator> MemoryRegion::GetListingItemIterator(int listing_item_start_index)
{
    // find the starting item by searching through the object tree
    auto tree_node = object_tree_root;
    while(tree_node) {
        assert(listing_item_start_index < tree_node->listing_item_count);

        if(listing_item_start_index < tree_node->left->listing_item_count) {
            // go left
            tree_node = tree_node->left;
        } else {
            // subtract left count and go right
            listing_item_start_index -= tree_node->left->listing_item_count;
            tree_node = tree_node->right;
        }

        if(tree_node->is_object) {
            shared_ptr<MemoryObjectTreeNode::iterator> it = make_shared<MemoryObjectTreeNode::iterator>();
            it->memory_region = shared_from_this();
            it->memory_object = tree_node->obj;
            it->listing_item_index = listing_item_start_index;

            // TODO this sucks. I need a better way to find the current address of the listing item
            // and I don't want to keep an address in the MemoryObject because in the future I want to
            // be able to insert new objects inbetween others, which would shift addresses around
            // for now, I know that I can determine MemoryObject sizes so we have to look up
            // the first object's address and save it in the iterator, then increment as necessary
            for(u32 region_offset = 0; region_offset < region_size; region_offset++) {
                if(object_refs[region_offset] == it->memory_object) {
                    it->region_offset = region_offset;
                    break;
                }
            }

            return it;
        }
    }

    assert(false);
    return nullptr;
}

shared_ptr<ListingItem>& MemoryObjectTreeNode::iterator::GetListingItem()
{
    return memory_object->listing_items[listing_item_index];
}

u32 MemoryObjectTreeNode::iterator::GetCurrentAddress()
{
    return region_offset + memory_region->GetBaseAddress();
}

MemoryObjectTreeNode::iterator& MemoryObjectTreeNode::iterator::operator++()
{
    // move onto the next listing item
    listing_item_index++;
    if(listing_item_index < memory_object->listing_items.size()) return *this;

    // if we run out within the current object, find the next object
    auto last_node = memory_object->parent.lock();
    auto current_node = last_node->parent.lock();

    // increment the region_offset by the size of the object
    region_offset += memory_object->GetSize();

    // go up until we're the left node
    while(current_node) {
        if(current_node->left == last_node) break;
        last_node = current_node;
        current_node = current_node->parent.lock();
    }

    // only happens when we are coming up the right side of the tree
    if(!current_node) { // ran out of nodes
        memory_object = nullptr;
    } else {
        // go right one
        current_node = current_node->right;

        // and go all the way down the left side of the tree
        while(current_node->left) current_node = current_node->left;

        // now we should be at a object node
        assert(current_node->is_object);

        // set up iterator and be done
        memory_object = current_node->obj;
        listing_item_index = 0;
    }

    return *this;
}

u32 MemoryObject::GetSize()
{
    switch(type) {
    case MemoryObject::TYPE_BYTE:
    case MemoryObject::TYPE_UNDEFINED:
        return 1;

    case MemoryObject::TYPE_WORD:
        return 2;

    default:
        assert(false);
        return 0;
    }
}

string MemoryObject::FormatInstructionField()
{
    stringstream ss;

    switch(type) {
    case MemoryObject::TYPE_UNDEFINED:
    case MemoryObject::TYPE_BYTE:
        ss << ".DB";
        break;

    case MemoryObject::TYPE_WORD:
        ss << ".DW";
        break;

    default:
        assert(false);
        break;
    }

    return ss.str();
}

string MemoryObject::FormatDataField(u32 /* internal_offset */)
{
    stringstream ss;

    ss << hex << setfill('0') << uppercase;

    switch(type) {
    case MemoryObject::TYPE_UNDEFINED:
    case MemoryObject::TYPE_BYTE:
        ss << "$" << setw(2) << (int)bval;
        break;

    case MemoryObject::TYPE_WORD:
        ss << "$" << setw(4) << hval;
        break;

    default:
        assert(false);
        break;
    }

    return ss.str();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProgramRomBank::ProgramRomBank(shared_ptr<System>& system, PROGRAM_ROM_BANK_LOAD _bank_load, PROGRAM_ROM_BANK_SIZE _bank_size) 
    : MemoryRegion(system), bank_load(_bank_load), bank_size(_bank_size) 
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
CharacterRomBank::CharacterRomBank(shared_ptr<System>& system, CHARACTER_ROM_BANK_LOAD _bank_load, CHARACTER_ROM_BANK_SIZE _bank_size)
    : MemoryRegion(system), bank_load(_bank_load), bank_size(_bank_size)
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
