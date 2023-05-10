#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_expressions.h"
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
        tree_node->listing_item_count = 0;
        if(tree_node->left) {
            _RecalculateListingItemCounts(tree_node->left);
            tree_node->listing_item_count += tree_node->left->listing_item_count;
        }

        if(tree_node->right) {
            _RecalculateListingItemCounts(tree_node->right);
            tree_node->listing_item_count += tree_node->right->listing_item_count;
        }
    }
}

void MemoryRegion::RecalculateListingItemCounts()
{
    _RecalculateListingItemCounts(object_tree_root);
}

void MemoryRegion::_SumListingItemCountsUp(shared_ptr<MemoryObjectTreeNode>& tree_node)
{
    while(tree_node) {
        tree_node->listing_item_count = 0;
        if(tree_node->left) tree_node->listing_item_count += tree_node->left->listing_item_count;
        if(tree_node->right) tree_node->listing_item_count += tree_node->right->listing_item_count;
        tree_node = tree_node->parent.lock();
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

    switch(obj->type) {
    case MemoryObject::TYPE_UNDEFINED:
    case MemoryObject::TYPE_BYTE:
    case MemoryObject::TYPE_WORD:
        obj->listing_items.push_back(make_shared<ListingItemData>(shared_from_this(), 0));
        break;

    case MemoryObject::TYPE_CODE:
        obj->listing_items.push_back(make_shared<ListingItemCode>(shared_from_this()));
        break;

    default:
        assert(false);
        break;
    }

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
        _InitializeFromData(tree_node->left , region_offset, &data[0], count / 2);

        // handle odd number of elements by putting the odd one on the right side
        tree_node->right = make_shared<MemoryObjectTreeNode>(tree_node);
        int fixed_count = (count / 2) + (count % 2);
        _InitializeFromData(tree_node->right, region_offset + count / 2, &data[count / 2], fixed_count);
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
    assert(count >= 2); // minimum region size, albeit silly
    object_tree_root->left  = make_shared<MemoryObjectTreeNode>(object_tree_root);
    object_tree_root->right = make_shared<MemoryObjectTreeNode>(object_tree_root);
    _InitializeFromData(object_tree_root->left , 0        , &data[0]        , count / 2);
    _InitializeFromData(object_tree_root->right, count / 2, &data[count / 2], (count / 2) + (count % 2));

    // first pass create listing items
    RecreateListingItems();
    RecalculateListingItemCounts();

    cout << "[MemoryRegion::InitializeWithData] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data for memory base 0x" << setw(4) << base_address << endl;
}

shared_ptr<MemoryObject> MemoryRegion::GetMemoryObject(GlobalMemoryLocation const& where)
{
    return object_refs[ConvertToRegionOffset(where.address)];
}

// to mark data as undefined, we just delete the current node and recreate new bytes in its place
bool MemoryRegion::MarkMemoryAsUndefined(GlobalMemoryLocation const& where)
{
    auto memory_object = GetMemoryObject(where);
    assert(memory_object);

    int size = memory_object->GetSize();
    u8* tmp = (u8*)_alloca(size);
    memory_object->Read(tmp, size);

    // save the is_object tree node before clearing memory_object from the tree
    auto tree_node = memory_object->parent.lock();

    // save this objects labels
    auto& labels = memory_object->labels;

    // remove memory_object from the tree first, this will correct listing item counts
    RemoveMemoryObjectFromTree(memory_object, true);

    // clear the is_object status of the tree node and build a tree with the data under it
    // this will update the object_refs[] array
    tree_node->is_object = false;
    u32 region_offset = ConvertToRegionOffset(where.address);
    _InitializeFromData(tree_node, region_offset, tmp, size);

    // copy the labels to the new object
    auto new_object = object_refs[region_offset];
    new_object->labels = labels;

    // recreate the listing items for each of the new memory objects
    for(u32 i = region_offset; i < region_offset + size; i++) {
        new_object = object_refs[i];
        RecreateListingItemsForMemoryObject(new_object, i);
    }

    // fix up this tree_node's listing item count
    _RecalculateListingItemCounts(tree_node);

    // and update the rest of the tree
    tree_node = tree_node->parent.lock();
    _SumListingItemCountsUp(tree_node);

    // the old memory_object will go out of scope here
    return true;
}

bool MemoryRegion::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    if((byte_count % 2) == 1) byte_count++;

    // Check to see if all selected memory can be converted
    for(u32 i = 0; i < byte_count; i += 2) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_WORD) continue;

        switch(memory_object->type) {
        case MemoryObject::TYPE_UNDEFINED:
        case MemoryObject::TYPE_BYTE:
        {
            auto next_object = GetMemoryObject(where + i + 1);

            if(next_object->type != MemoryObject::TYPE_UNDEFINED && next_object->type != MemoryObject::TYPE_BYTE) {
                cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << "+1 cannot be converted to a word (currently type " << memory_object->type << ")" << endl;
                return false;
            }

            break;
        }

        default:
            cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << " cannot be converted to a word (currently type " << memory_object->type << ")" << endl;
            return false;
        }
    }

    // OK, convert them
    for(u32 i = 0; i < byte_count; i += 2) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_WORD) continue;

        switch(memory_object->type) {
        case MemoryObject::TYPE_UNDEFINED:
        case MemoryObject::TYPE_BYTE:
        {
            auto next_object = GetMemoryObject(where + i + 1);

            RemoveMemoryObjectFromTree(next_object);

            // change the current object to a word
            memory_object->type = MemoryObject::TYPE_WORD;
            memory_object->hval = (u16)memory_object->bval | ((u16)next_object->bval << 8);

            // update the object_refs
            u32 x = ConvertToRegionOffset((where + i + 1).address);
            object_refs[x] = memory_object;

            // listings may have changed
            _UpdateMemoryObject(memory_object, ConvertToRegionOffset(where.address));
            break;
        }

        default:
            assert(false);
            return false;
        }
    }

    return true;
}

bool MemoryRegion::MarkMemoryAsCode(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Check to see if all selected memory can be converted
    for(u32 i = 0; i < byte_count; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type != MemoryObject::TYPE_BYTE && memory_object->type != MemoryObject::TYPE_UNDEFINED) {
            cout << "[MemoryRegion::MarkMemoryAsWords] address " << (where + i) << " cannot be converted to code (currently type " << memory_object->type << ")" << endl;
            return false;
        }
    }

    // The first object will be changed into code
    auto inst = GetMemoryObject(where);

    // don't have to set the opcode because it's already the bval in the union
    // inst->code.opcode = inst->bval;

    // get the operand bytes and remove them
    for(u32 i = 1; i < byte_count; i++) {
        auto operand_object = GetMemoryObject(where + i);
        assert(operand_object->type == MemoryObject::TYPE_BYTE || operand_object->type == MemoryObject::TYPE_UNDEFINED);
        RemoveMemoryObjectFromTree(operand_object);

        // TODO steal data from operand_object
        inst->code.operands[i-1] = operand_object->bval;

        // update the object_refs
        u32 x = ConvertToRegionOffset((where + i).address);
        object_refs[x] = inst;
    }

    // convert the inst to TYPE_CODE and update the tree
    inst->type = MemoryObject::TYPE_CODE;
    UpdateMemoryObject(where);

    return true;
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

void MemoryRegion::_UpdateMemoryObject(shared_ptr<MemoryObject>& memory_object, u32 region_offset)
{
    // recreate the listing items for this one object
    RecreateListingItemsForMemoryObject(memory_object, region_offset);

    // propagate up the tree the changes
    auto current_node = memory_object->parent.lock();
    current_node->listing_item_count = memory_object->listing_items.size(); // update the is_object node
    current_node = current_node->parent.lock();
    _SumListingItemCountsUp(current_node);
}

void MemoryRegion::UpdateMemoryObject(GlobalMemoryLocation const& where)
{
    u32 region_offset = ConvertToRegionOffset(where.address);
    auto memory_object = object_refs[region_offset];
    _UpdateMemoryObject(memory_object, region_offset);
}

// save_tree_node means we don't delete the is_object tree node, so that the caller can use it to build a new subtree
void MemoryRegion::RemoveMemoryObjectFromTree(shared_ptr<MemoryObject>& memory_object, bool save_tree_node)
{
    // propagate up the tree the changes
    auto last_node = memory_object->parent.lock();
    auto current_node = last_node;

    memory_object->parent.reset();

    // clear the pointer to the memory_object
    last_node->obj = nullptr;

    // sometimes we don't want to free the tree node
    if(!save_tree_node) {
        do {
            // clear the pointer to the is_object node
            current_node = last_node->parent.lock();
            assert(current_node);
            if(current_node->left == last_node) {
                current_node->left = nullptr;
            } else {
                current_node->right = nullptr;
            }

            last_node = current_node;
        } while(!current_node->left && !current_node->right); // uh-oh, need to remove this branch entirely
    }

    // update the listing item count
    _SumListingItemCountsUp(current_node);
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

// Returns the listing item index in the whole tree given the memory object
// Trivally, go up the whole tree adding left nodes
u32 MemoryRegion::GetListingItemIndexForMemoryObject(shared_ptr<MemoryObject> const& memory_object)
{
    u32 index = 0;

    auto previous_node = memory_object->parent.lock();
    auto current_node = previous_node->parent.lock();

    do {
        if(current_node->left && current_node->left != previous_node) index += current_node->left->listing_item_count;
        previous_node = current_node;
        current_node = current_node->parent.lock();
    } while(current_node);

    return index;
}

// use a binary search through the object_refs to find the first region_offset where listing_item_index is located
u32 MemoryRegion::FindRegionOffsetForListingItem(int listing_item_index)
{
    // binary search object_refs[] for the object node that contains listing_item_index
    u32 low = 0, high = GetRegionSize();
    u32 region_offset;

    do {
        region_offset = low + (high - low) / 2;

        auto memory_object = object_refs[region_offset];
        u32 i = GetListingItemIndexForMemoryObject(memory_object); // kinda heavy but oh well

        // if the listing_item_index is in this memory object, break out
        if(listing_item_index >= i && listing_item_index < (i + memory_object->listing_items.size())) break;

        // otherwise, go lower or higher
        if(listing_item_index < i) {
            high = region_offset;
        } else {
            low = region_offset;
        }
    } while(high != low);

    // but some addresses point to the same object, so we need to back up until we get the
    // first address that points to the object
    auto memory_object = object_refs[region_offset]; // this is the correct object, but maybe not the correct region_offset
    while(region_offset != 0 && object_refs[region_offset-1].get() == memory_object.get()) --region_offset;

    return region_offset;
}

shared_ptr<MemoryObjectTreeNode::iterator> MemoryRegion::GetListingItemIterator(int listing_item_start_index)
{
    u32 listing_item_index = listing_item_start_index;

    // find the starting item by searching through the object tree
    auto tree_node = object_tree_root;
    while(tree_node) {
        assert(listing_item_index < tree_node->listing_item_count);

        if(tree_node->left && listing_item_index < tree_node->left->listing_item_count) {
            // go left
            tree_node = tree_node->left;
        } else {
            // subtract left count (if any) and go right instead
            if(tree_node->left) {
                listing_item_index -= tree_node->left->listing_item_count;
            }

            tree_node = tree_node->right;
        }

        if(tree_node->is_object) {
            shared_ptr<MemoryObjectTreeNode::iterator> it = make_shared<MemoryObjectTreeNode::iterator>();
            it->memory_region = shared_from_this();
            it->memory_object = tree_node->obj;
            it->listing_item_index = listing_item_index;
            it->disassembler = parent_system.lock()->GetDisassembler();

            // TODO this sucks. I need a better way to find the current address of the listing item
            // and I don't want to keep an address in the MemoryObject because in the future I want to
            // be able to insert new objects inbetween others, which would shift addresses around
            // for now, I know that I can determine MemoryObject sizes so we have to look up
            // the first object's address and save it in the iterator, then increment as necessary
            it->region_offset = FindRegionOffsetForListingItem(listing_item_start_index);

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
    region_offset += memory_object->GetSize(disassembler);

    // go up until we're the left node and there's a right one to go down
    while(current_node) {
        if(current_node->left == last_node && current_node->right) break;
        last_node = current_node;
        current_node = current_node->parent.lock();
    }

    // only happens when we are coming up the right side of the tree
    if(!current_node) { // ran out of nodes
        memory_object = nullptr;
        //cout << "ran out of objects, hopefully you aren't trying to show more, current region_offset = 0x" << hex << region_offset << endl;
    } else {
        // go right one
        current_node = current_node->right;

        do {
            // and go all the way down the left side of the tree
            while(current_node->left) current_node = current_node->left;

            // if we get to a null left child, go right one and repeat going left
            if(!current_node->is_object) {
                assert(current_node->right); // should never happen, one child should always be non-null, otherwise there was a bug in RemoveMemoryObjectFromTree
                current_node = current_node->right;
            }
        } while(!current_node->is_object);

        // now we should be at a object node
        assert(current_node->is_object);

        // set up iterator and be done
        memory_object = current_node->obj;
        listing_item_index = 0;
    }

    return *this;
}

u32 MemoryObject::GetSize(shared_ptr<Disassembler> disassembler)
{
    switch(type) {
    case MemoryObject::TYPE_BYTE:
    case MemoryObject::TYPE_UNDEFINED:
        return 1;

    case MemoryObject::TYPE_WORD:
        return 2;

    case MemoryObject::TYPE_CODE:
        return disassembler->GetInstructionSize(code.opcode);

    default:
        assert(false);
        return 0;
    }
}

void MemoryObject::Read(u8* buf, int count)
{
    assert(count <= GetSize());
    switch(type) {
    case MemoryObject::TYPE_BYTE:
    case MemoryObject::TYPE_UNDEFINED:
    case MemoryObject::TYPE_WORD:
    case MemoryObject::TYPE_CODE:
        memcpy(buf, (void*)&bval, count);
        break;

    default:
        assert(false);
        break;
    }
}

string MemoryObject::FormatInstructionField(shared_ptr<Disassembler> disassembler)
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

    case MemoryObject::TYPE_CODE:
        ss << disassembler->GetInstruction(code.opcode);
        break;

    default:
        assert(false);
        break;
    }

    return ss.str();
}

string MemoryObject::FormatDataField(u32 /* internal_offset */, shared_ptr<Disassembler> disassembler)
{
    stringstream ss;

    // if there's an operand expression, display that, otherwise format a default expression
    if(operand_expression) {
        ss << *operand_expression;
    } else {
        ss << hex << setfill('0') << uppercase;

        switch(type) {
        case MemoryObject::TYPE_UNDEFINED:
        case MemoryObject::TYPE_BYTE:
            ss << "$" << setw(2) << (int)bval;
            break;

        case MemoryObject::TYPE_WORD:
            ss << "$" << setw(4) << hval;
            break;

        case MemoryObject::TYPE_CODE:
            ss << disassembler->FormatOperand(code.opcode, code.operands);
            break;

        default:
            assert(false);
            break;
        }
    }

    return ss.str();
}

u8 MemoryRegion::ReadByte(GlobalMemoryLocation const& where)
{
    return 0;
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
