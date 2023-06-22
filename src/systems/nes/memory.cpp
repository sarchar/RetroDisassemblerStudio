// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "magic_enum.hpp"
#include "util.h"

#include "systems/nes/comment.h"
#include "systems/nes/disasm.h"
#include "systems/nes/expressions.h"
#include "systems/nes/label.h"
#include "systems/nes/system.h"

#include "windows/nes/listingitems.h"
#include "windows/nes/project.h"

using namespace std;

namespace Systems::NES {

bool GlobalMemoryLocation::Save(std::ostream& os, std::string& errmsg) const
{
    WriteVarInt(os, address);
    assert(sizeof(is_chr) == 1);
    os.write((char*)&is_chr, sizeof(is_chr));
    WriteVarInt(os, prg_rom_bank);
    WriteVarInt(os, chr_rom_bank);
    if(!os.good()) {
        errmsg = "Error writing GlobalMemoryLocation";
        return false;
    }
    return true;
}

bool GlobalMemoryLocation::Load(std::istream& is, std::string& errmsg)
{
    address = ReadVarInt<u16>(is);
    is.read((char*)&is_chr, sizeof(is_chr));
    prg_rom_bank = ReadVarInt<u16>(is);
    chr_rom_bank = ReadVarInt<u16>(is);
    if(!is.good()) {
        errmsg = "Error reading GlobalMemoryLocation";
        return false;
    }
    //cout << *this << endl;
    return true;
}

MemoryRegion::MemoryRegion(shared_ptr<System>& _parent_system, string const& _name) 
    : name(_name)
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

    if(obj->default_blank_line) {
        // create a blank line inbetween other memory and labels, unless at the start of the bank
        // TODO or if it's a local label
        obj->blank_lines = (obj->labels.size() && region_offset != 0) ? 1 : 0;
    }

    for(int i = 0; i < obj->blank_lines; i++) {
        obj->listing_items.push_back(make_shared<Windows::NES::ListingItemBlankLine>());
    }

    // create the pre comment
    if(obj->comments.pre) {
        for(int i = 0; i < obj->comments.pre->GetLineCount(); i++) {
            obj->listing_items.push_back(make_shared<Windows::NES::ListingItemCommentOnly>(MemoryObject::COMMENT_TYPE_PRE, i));
        }
    }

    // create an item for each label
    for(int nth = 0; nth < obj->labels.size(); nth++) {
        auto& label = obj->labels[nth];
        obj->listing_items.push_back(make_shared<Windows::NES::ListingItemLabel>(label, nth));
    }

    // the primary index is used to focus on code or data when moving to locations in the listing windows
    obj->primary_listing_item_index = obj->listing_items.size();

    // create the primary memory object line
    {
        int index = 0;
        obj->listing_items.push_back(make_shared<Windows::NES::ListingItemPrimary>(index++));

        // add EOL comments not including the first (printed in the Primary item)
        if(obj->comments.eol) {
            for(int i = 1; i < obj->comments.eol->GetLineCount(); i++) {
                obj->listing_items.push_back(make_shared<Windows::NES::ListingItemCommentOnly>(MemoryObject::COMMENT_TYPE_EOL, i));
            }
        }
    }

    // create the post comment
    if(obj->comments.post) {
        for(int i = 0; i < obj->comments.pre->GetLineCount(); i++) {
            obj->listing_items.push_back(make_shared<Windows::NES::ListingItemCommentOnly>(MemoryObject::COMMENT_TYPE_POST, i));
        }
    }
}

void MemoryRegion::_InitializeFromData(shared_ptr<MemoryObjectTreeNode>& tree_node, u32 region_offset, int count)
{
    // stop the iteration when there's one byte left
    if(count == 1) {
        tree_node->is_object = true;

        // create the object
        shared_ptr<MemoryObject> obj = make_shared<MemoryObject>();
        obj->parent = tree_node;

        // set the data
        obj->type = MemoryObject::TYPE_UNDEFINED;
        obj->backed = true;
        obj->data_ptr = &flat_memory[region_offset];

        // set the element in the node
        tree_node->obj = obj;

        // and create the memory address reference to the object
        object_refs[region_offset] = obj;
    } else {
        // initialize the tree by splitting the data into left and right halves
        tree_node->left  = make_shared<MemoryObjectTreeNode>(tree_node);
        _InitializeFromData(tree_node->left , region_offset, count / 2);

        // handle odd number of elements by putting the odd one on the right side
        tree_node->right = make_shared<MemoryObjectTreeNode>(tree_node);
        int fixed_count = (count / 2) + (count % 2);
        _InitializeFromData(tree_node->right, region_offset + count / 2, fixed_count);
    }
}

// I don't like the duplicated code between this and _InitializeFromData
void MemoryRegion::_InitializeEmpty(shared_ptr<MemoryObjectTreeNode>& tree_node, u32 region_offset, int count)
{
    // stop the iteration when there's one byte left
    if(count == 1) {
        tree_node->is_object = true;

        // create the object
        shared_ptr<MemoryObject> obj = make_shared<MemoryObject>();
        obj->parent = tree_node;

        // set the data info
        obj->type = MemoryObject::TYPE_UNDEFINED;
        obj->backed = false;
        obj->data_ptr = nullptr;

        // set the element in the node
        tree_node->obj = obj;

        // and create the memory address reference to the object
        object_refs[region_offset] = obj;
    } else {
        // initialize the tree by splitting the data into left and right halves
        tree_node->left  = make_shared<MemoryObjectTreeNode>(tree_node);
        _InitializeEmpty(tree_node->left , region_offset, count / 2);

        // handle odd number of elements by putting the odd one on the right side
        tree_node->right = make_shared<MemoryObjectTreeNode>(tree_node);
        int fixed_count = (count / 2) + (count % 2);
        _InitializeEmpty(tree_node->right, region_offset + count / 2, fixed_count);
    }
}

void MemoryRegion::_ReinializeFromObjectRefs(shared_ptr<MemoryObjectTreeNode>& tree_node, vector<int> const& objmap, u32 uid_start, int count)
{
    // stop the iteration when there's one byte left
    if(count == 1) {
        tree_node->is_object = true;

        // don't create or the object, since we already have it
        auto obj = object_refs[objmap[uid_start]];

        // set the parent
        obj->parent = tree_node;

        // and the obj pointer
        tree_node->obj = obj;

    } else {
        // initialize the tree by splitting the data into left and right halves
        tree_node->left  = make_shared<MemoryObjectTreeNode>(tree_node);
        _ReinializeFromObjectRefs(tree_node->left , objmap, uid_start, count / 2);

        // handle odd number of elements by putting the odd one on the right side
        tree_node->right = make_shared<MemoryObjectTreeNode>(tree_node);
        int fixed_count = (count / 2) + (count % 2);
        _ReinializeFromObjectRefs(tree_node->right, objmap, uid_start + count / 2, fixed_count);
    }
}


void MemoryRegion::InitializeFromData(u8* data, int count)
{
    assert(count == region_size); // can only initialize the memory region tree with an exact number of bytes

    // Kill all content blocks and references
    Erase();

    // the refs list is a object lookup by address map, and will always be the size of the memory region
    object_refs.resize(count);

    // allocate storage for the flat memory and copy it over
    if(flat_memory) delete [] flat_memory;
    flat_memory = new u8[count];
    memcpy(flat_memory, data, count);

    // We need a root for the tree first and foremost
    object_tree_root = make_shared<MemoryObjectTreeNode>(nullptr);

    // initialize the tree by splitting the data into left and right halves
    assert(count >= 2); // minimum region size, albeit silly
    object_tree_root->left  = make_shared<MemoryObjectTreeNode>(object_tree_root);
    object_tree_root->right = make_shared<MemoryObjectTreeNode>(object_tree_root);
    _InitializeFromData(object_tree_root->left , 0        ,  count / 2);
    _InitializeFromData(object_tree_root->right, count / 2,  (count / 2) + (count % 2));

    // first pass create listing items
    RecreateListingItems();
    RecalculateListingItemCounts();

    cout << "[MemoryRegion::InitializeWithData] set $" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data for memory base $" << setw(4) << base_address << endl;
}

void MemoryRegion::ReinitializeFromObjectRefs()
{
    // we need a mapping from unique object index to offset in the region
    // that requires a once-over the entire list of objects
    std::vector<int> objmap;

    auto current_object = object_refs[0];
    objmap.push_back(0);
    for(u32 offset = 0; offset < region_size; offset++) {
        auto next_object = object_refs[offset];
        if(next_object != current_object) {
            current_object = next_object;
            objmap.push_back(offset);
        }
    }

    u32 count = objmap.size();

    // We need a root for the tree first and foremost
    object_tree_root = make_shared<MemoryObjectTreeNode>(nullptr);

    // initialize the tree by splitting the data into left and right halves
    assert(count >= 2); // minimum object count
    object_tree_root->left  = make_shared<MemoryObjectTreeNode>(object_tree_root);
    object_tree_root->right = make_shared<MemoryObjectTreeNode>(object_tree_root);
    _ReinializeFromObjectRefs(object_tree_root->left , objmap, 0        , count / 2);
    _ReinializeFromObjectRefs(object_tree_root->right, objmap, count / 2, (count / 2) + (count % 2));

    // creating the listing items and recalculate the tree
    RecreateListingItems();
    RecalculateListingItemCounts();

    cout << "[MemoryRegion::ReinitializeFromObjectRefs] processed " << count << " objects" << endl;
}

void MemoryRegion::InitializeEmpty()
{
    // Kill all content blocks and references
    Erase();

    // the refs list is a object lookup by address map, and will always be the size of the memory region
    int count = (int)GetRegionSize();
    object_refs.resize(count);

    // We need a root for the tree first and foremost
    object_tree_root = make_shared<MemoryObjectTreeNode>(nullptr);

    // initialize the tree by splitting the data into left and right halves
    assert(count >= 2); // minimum region size, albeit silly
    object_tree_root->left  = make_shared<MemoryObjectTreeNode>(object_tree_root);
    object_tree_root->right = make_shared<MemoryObjectTreeNode>(object_tree_root);
    _InitializeEmpty(object_tree_root->left , 0        , count / 2);
    _InitializeEmpty(object_tree_root->right, count / 2, (count / 2) + (count % 2));

    // first pass create listing items
    RecreateListingItems();
    RecalculateListingItemCounts();

    cout << "[MemoryRegion::InitializeEmpty] non-backed memory initialized at $" 
         << hex << uppercase << setfill('0') << setw(4) << base_address << endl;
}

shared_ptr<MemoryObject> MemoryRegion::GetMemoryObject(GlobalMemoryLocation const& where, int* offset)
{
    int region_offset = ConvertToRegionOffset(where.address);
    auto ret = object_refs[region_offset];

    if(offset != NULL) {
        *offset = 0;

        auto cur = object_refs[region_offset];
        while(cur == ret && region_offset > 0) {
            region_offset -= 1;
            cur = object_refs[region_offset];
            if(ret == cur) (*offset) += 1;
        };
    }

    return ret;
}

// to mark data as undefined, we just delete the current node and recreate new bytes in its place
bool MemoryRegion::MarkMemoryAsUndefined(GlobalMemoryLocation const& where, u32 byte_count)
{
    for(u32 offset = 0; offset < byte_count;) {
        auto memory_object = GetMemoryObject(where + offset);
        assert(memory_object);

        // Don't convert already undefined objects
        if(memory_object->type == MemoryObject::TYPE_UNDEFINED) {
            offset += memory_object->GetSize();
            continue;
        }

        int size = memory_object->GetSize();

        // save the is_object tree node before clearing memory_object from the tree
        auto tree_node = memory_object->parent.lock();

        // save this objects labels
        auto& labels = memory_object->labels;

        // clear any references this object is making
        memory_object->RemoveReferences(where + offset);

        // remove memory_object from the tree first, this will correct listing item counts
        RemoveMemoryObjectFromTree(memory_object, true);

        // clear the is_object status of the tree node and build a tree with the data under it
        // this will update the object_refs[] array
        tree_node->is_object = false;
        u32 region_offset = ConvertToRegionOffset(where.address + offset);
        if(memory_object->backed) {
            // we don't need to save the memory object's data_ptr as it is reinitialized 
            // in _InitializeFromData
            _InitializeFromData(tree_node, region_offset, size);
        } else {
            _InitializeEmpty(tree_node, region_offset, size);
        }

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

        // move past this object
        offset += size;
    }

    // the old memory_object will go out of scope here
    return true;
}

bool MemoryRegion::MarkMemoryAsBytes(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Check to see if all selected memory is undefined. other data cannot be converted
    for(u32 i = 0; i < byte_count; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_BYTE) continue;

        if(memory_object->type != MemoryObject::TYPE_UNDEFINED) {
            cout << "[MemoryRegion::MarkMemoryAsBytes] address 0x" << (where.address + i) 
                 << " cannot be converted to a byte (currently type " 
                 << magic_enum::enum_name(memory_object->type) << ")" << endl;
            return false;
        }
    }

    // OK, convert them
    for(u32 i = 0; i < byte_count; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_BYTE) continue;
        assert(memory_object->type == MemoryObject::TYPE_UNDEFINED);

        // change the object to a byte
        memory_object->type = MemoryObject::TYPE_BYTE;

        // object_refs don't change

        // listing items may have changed
        _UpdateMemoryObject(memory_object, ConvertToRegionOffset(where.address));
    }

    return true;
}

bool MemoryRegion::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Round up
    if((byte_count % 2) == 1) byte_count++;

    // Check to see if all selected memory is undefined. other data cannot be converted
    for(u32 i = 0; i < byte_count; i += 2) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_WORD) continue;

        switch(memory_object->type) {
        case MemoryObject::TYPE_UNDEFINED: { // need two TYPE_UNDEFINEDs together
            auto next_object = GetMemoryObject(where + i + 1);

            if(next_object->type != MemoryObject::TYPE_UNDEFINED) {
                cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << "+1 cannot be converted to a word (currently type " << magic_enum::enum_name(next_object->type) << ")" << endl;
                return false;
            }

            break;
        }

        default: // all other types aren't convertable
            cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << " cannot be converted to a word (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
            return false;
        }
    }

    // OK, convert them
    for(u32 i = 0; i < byte_count; i += 2) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type == MemoryObject::TYPE_WORD) continue;
        assert(memory_object->type == MemoryObject::TYPE_UNDEFINED);

        // remove the high byte from the object tree
        auto next_object = GetMemoryObject(where + i + 1);
        RemoveMemoryObjectFromTree(next_object);

        // change the current object to a word, data_ptr doesn't change
        memory_object->type = MemoryObject::TYPE_WORD;

        // update the object_refs
        u32 x = ConvertToRegionOffset((where + i + 1).address);
        object_refs[x] = memory_object;

        // listing items have changed
        _UpdateMemoryObject(memory_object, ConvertToRegionOffset(where.address));
    }

    return true;
}

// mark one piece of memory as an instruction
bool MemoryRegion::MarkMemoryAsCode(GlobalMemoryLocation const& where)
{
    auto system = parent_system.lock();
    if(!system) return false;

    auto disassembler = system->GetDisassembler();

    // The first object will be changed into code
    auto inst = GetMemoryObject(where);

    // regardless of data type, we can always read *data_ptr if the memory is backed
    assert(inst->backed);
    int instruction_size = disassembler->GetInstructionSize(*inst->data_ptr);

    // Check to see if all selected memory can be converted
    // opcode and operands must be TYPE_UNDEFINED to convert
    for(u32 i = 0; i < instruction_size; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type != MemoryObject::TYPE_UNDEFINED) {
            cout << "[MemoryRegion::MarkMemoryAsCode] address " << (where + i) << " cannot be converted to code (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
            return false;
        }
    }

    // don't have to change data_ptr as it already points at the opcode

    // remove the operand objects from the tree
    for(u32 i = 1; i < instruction_size; i++) {
        auto operand_object = GetMemoryObject(where + i);
        assert(operand_object->type == MemoryObject::TYPE_UNDEFINED);
        RemoveMemoryObjectFromTree(operand_object);

        // don't have to copy the operands as they're sequential in memory from inst->data_ptr

        // update the object_refs
        u32 x = ConvertToRegionOffset((where + i).address);
        object_refs[x] = inst;
    }

    // convert the inst to TYPE_CODE and update the tree and object
    inst->type = MemoryObject::TYPE_CODE;
    UpdateMemoryObject(where);

    return true;
}

bool MemoryRegion::MarkMemoryAsString(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Check to see if all selected memory can be converted
    for(u32 i = 0; i < byte_count; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type != MemoryObject::TYPE_UNDEFINED) {
            cout << "[MemoryRegion::MarkMemoryAsString] address " << (where + i) << " cannot be converted to code (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
            return false;
        }
    }

    // The first object will be changed into the string
    auto str_object = GetMemoryObject(where);

    // set the string length
    str_object->string_length = byte_count;

    // remove the rest of the objects from the tree
    for(u32 i = 1; i < byte_count; i++) {
        auto next_byte_object = GetMemoryObject(where + i);
        assert(next_byte_object->type == MemoryObject::TYPE_UNDEFINED);
        RemoveMemoryObjectFromTree(next_byte_object);

        // update the object_refs
        u32 x = ConvertToRegionOffset((where + i).address);
        object_refs[x] = str_object;
    }

    // convert the str_object to TYPE_STRING and update the tree and object
    str_object->type = MemoryObject::TYPE_STRING;
    UpdateMemoryObject(where);

    return true;
}

bool MemoryRegion::MarkMemoryAsEnum(GlobalMemoryLocation const& where, u32 byte_count, shared_ptr<Enum> const& enum_type)
{
    int enum_size = enum_type->GetSize();

    // Check to see if all selected memory is undefined. other data cannot be converted
    for(int loop = 0; loop < 2; loop++) {
        for(u32 i = 0; i < byte_count; i += enum_size) {
            auto memory_object = GetMemoryObject(where + i);

            // skip over elements of this type of enum only
            if(memory_object->type == MemoryObject::TYPE_ENUM
               && memory_object->user_type.index() == 1
               && get<shared_ptr<Enum>>(memory_object->user_type).get() == enum_type.get()) continue;

            // all enum_size bytes need to be undefined
            for(int j = 0; j < enum_size; j++) {
                auto test_object = GetMemoryObject(where + i + j);
                if(test_object->type != MemoryObject::TYPE_UNDEFINED) {
                    cout << "[MemoryRegion::MarkMemoryAsEnum] address 0x" << (where.address + i + j) 
                         << " cannot be converted to type enum " << enum_type->GetName()
                         << " (currently type " 
                         << magic_enum::enum_name(test_object->type) << ")" << endl;
                    return false;
                }
            }

            // first time through the loop we just verify TYPE_UNDEFINED
            if(loop == 0) continue; 

            // change the object to a byte
            memory_object->type = MemoryObject::TYPE_ENUM;
            memory_object->user_type = enum_type;

            // remove enum_size-1 objects from the tree
            for(int j = 1; j < enum_size; j++) {
                auto next_object = GetMemoryObject(where + i + j);
                RemoveMemoryObjectFromTree(next_object);

                // set the object_refs to point to the first object
                u32 x = ConvertToRegionOffset((where + i + j).address);
                object_refs[x] = memory_object;
            }

            // listing items may have changed
            _UpdateMemoryObject(memory_object, ConvertToRegionOffset(where.address));

            // TYPE_UNDEFINED doesn't reference other objects, but we need to set references
            // to the newly assigned enum
            NoteReferences(where);
        }
    }
    return true;
}

void MemoryRegion::SetOperandExpression(GlobalMemoryLocation const& where, std::shared_ptr<Expression> const& expr)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->RemoveReferences(where); // clear any references the previous operand expression referred to
        memory_object->operand_expression = expr;
        memory_object->NoteReferences(where);   // mark the new ones
    }
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

bool MemoryRegion::GetGlobalMemoryLocation(u32 offset, GlobalMemoryLocation* out)
{
    if(offset >= GetRegionSize()) return false;
    *out = GlobalMemoryLocation {
        .address = (u16)((base_address + offset) & 0xFFFF),
    };
    return true;
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


//void MemoryRegion::CreateLabel(GlobalMemoryLocation const& where, string const& label)
void MemoryRegion::ApplyLabel(shared_ptr<Label>& label)
{
    auto where = label->GetMemoryLocation();
    u32 region_offset = ConvertToRegionOffset(where.address);
    auto memory_object = object_refs[region_offset];

    // add the label
    label->SetIndex(memory_object->labels.size());
    memory_object->labels.push_back(label);

    // update the object
    UpdateMemoryObject(where);
}

int MemoryRegion::DeleteLabel(shared_ptr<Label> const& label)
{
    int ret = -1;
    auto where = label->GetMemoryLocation();
    if(auto memory_object = GetMemoryObject(where)) {
        ret = memory_object->DeleteLabel(label);
        if(memory_object->blank_lines == 0 && memory_object->labels.size() == 0) {
            memory_object->default_blank_line = true;
        }
        UpdateMemoryObject(where);
    }

    return ret;
}

void MemoryRegion::NextLabelReference(GlobalMemoryLocation const& where)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->NextLabelReference(where);
    }
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

shared_ptr<Windows::NES::ListingItem>& MemoryObjectTreeNode::iterator::GetListingItem()
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

struct MemoryObject::LabelCreatedData {
    GlobalMemoryLocation target;
    signal_connection created_connection;
    signal_connection deleted_connection;
};

void MemoryObject::NoteReferences(GlobalMemoryLocation const& where)
{
    // Create the specific memory object references
    auto operand_ref = make_shared<MemoryObjectOperandReference>(where);

    // Note references based on the object type
    if(type == TYPE_ENUM) {
        auto& enum_type = get<shared_ptr<Enum>>(user_type);
        auto type_ref = make_shared<MemoryObjectTypeReference>(where);
        enum_type->NoteReference(type_ref);

        // determine the element value
        s64 enum_element_value = data_ptr[0];
        if(enum_type->GetSize() == 2) enum_element_value |= (s64)((u16)data_ptr[1] << 8);
        else assert(enum_type->GetSize() == 1);

        // for enum types, note the reference on the operand value
        auto enum_elements = enum_type->GetElementsByValue(enum_element_value);
        if(enum_elements.size()) {
            enum_element = enum_elements[0];
            enum_element->NoteReference(operand_ref);
        } else {
            // if there's no enum element for this value, watch for new elements to be added
            // or changed
            user_type_conn1 = enum_type->element_added->connect(
                [this, operand_ref, enum_element_value](shared_ptr<EnumElement> const& ee) {
                    if(!enum_element && ee->cached_value == enum_element_value) {
                        enum_element = ee;
                        enum_element->NoteReference(operand_ref);
                    }
                });
            user_type_conn2 = enum_type->element_changed->connect(
                [this, operand_ref, enum_element_value](shared_ptr<EnumElement> const& ee, string const&, s64) {
                    if(!enum_element && ee->cached_value == enum_element_value) {
                        enum_element = ee;
                        enum_element->NoteReference(operand_ref);
                    }
                });
        }
    }

    // if there's no operand expresion, there are no references
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // Explore operand_expression and mark each referenced define and label that we're referring to
    auto cb = [this, &operand_ref](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->NoteReference(operand_ref);
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->NoteReference(operand_ref);
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // tell the expression node to update the reference to the label
            label_node->Update();
            auto label = label_node->GetLabel();
            if(label) label->NoteReference(operand_ref);

            // and create a callback for any label created at the target address
            auto system = GetSystem();
            GlobalMemoryLocation const& target = label_node->GetTarget();

            label_connections.push_back(make_shared<LabelCreatedData>(LabelCreatedData {
                .target = target,
                .created_connection = system->LabelCreatedAt(target)->connect(
                        [this, operand_ref](shared_ptr<Label> const& label, bool was_user_created) {
                            // this will notify the new label that we're referring to it. if a different label is created
                            // at the same address, this won't reference that label since the current expression node already has
                            // a label
                            label->NoteReference(operand_ref);
                        }),
                .deleted_connection = system->LabelDeletedAt(target)->connect(
                        [this, operand_ref, label_node](shared_ptr<Label> const& label, int nth) {
                            if(label_node->GetNth() == nth) { // only if the deleted label is the one we are referring to
                                label->RemoveReference(operand_ref);
                                label_node->Reset();
                                label_node->Update();
                            }
                        })
            }));
        }
        return true;
    };

    // TODO clear all label_created signal handlers before recreating them
    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen

    // note references in comments as well
    if(comments.pre) comments.pre->NoteReferences();
    if(comments.eol) comments.eol->NoteReferences();
    if(comments.post) comments.post->NoteReferences();
}

void MemoryObject::RemoveReferences(GlobalMemoryLocation const& where)
{
    auto system = GetSystem();

    // Clear all the label_created signal connections
    for(auto& data : label_connections) {
        data->created_connection->disconnect();
        data->deleted_connection->disconnect();
        system->LabelCreatedAtRemoved(data->target);
        system->LabelDeletedAtRemoved(data->target);
    }

    label_connections.clear();

    // Refereceable needs shared_ptr
    auto operand_ref = make_shared<MemoryObjectOperandReference>(where);

    // Clear references based on the object type
    if(type == TYPE_ENUM) {
        auto& enum_type = get<shared_ptr<Enum>>(user_type);
        auto type_ref = make_shared<MemoryObjectTypeReference>(where);
        enum_type->RemoveReference(type_ref);

        // for enum types, note the reference on the operand value
        if(enum_element) {
            enum_element->RemoveReference(operand_ref);
            enum_element = nullptr;
        } else {
            user_type_conn1->disconnect();
            user_type_conn2->disconnect();
        }
    }

    // if there's no operand expresion, there are no references
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // Explore operand_expression and tell each referenced object we no longer care about them
    auto cb = [&operand_ref](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->RemoveReference(operand_ref);
        } else if(auto ee_node = dynamic_pointer_cast<ExpressionNodes::EnumElement>(node)) {
            ee_node->GetEnumElement()->RemoveReference(operand_ref);
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            auto label = label_node->GetLabel();
            if(label) label->RemoveReference(operand_ref);
        }
        return true;
    };

    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

int MemoryObject::DeleteLabel(std::shared_ptr<Label> const& label)
{
    assert(label->GetIndex() < labels.size() && label->GetString() == labels[label->GetIndex()]->GetString());
    int nth = label->GetIndex();
    labels.erase(labels.begin() + nth);

    for(; nth < labels.size(); nth++) {
        labels[nth]->SetIndex(nth);
    }

    return nth;
}

// Change to the next label at a given address
void MemoryObject::NextLabelReference(GlobalMemoryLocation const& where)
{
    // if there's no operand expresion, there are no labels
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // create the reference object
    auto operand_ref = make_shared<MemoryObjectOperandReference>(where);

    // Explore the expression, calling Increment on the first and then bailing
    auto cb = [&operand_ref](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // convert the label_node to a constant node
            auto label = label_node->GetLabel();
            label->RemoveReference(operand_ref);
            label_node->NextLabel();
            label = label_node->GetLabel();
            label->NoteReference(operand_ref);
        }
        return true;
    };

    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
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
        return disassembler->GetInstructionSize(*data_ptr);

    case MemoryObject::TYPE_STRING:
        return string_length;

    case MemoryObject::TYPE_ENUM: {
        auto enum_type = get<shared_ptr<Enum>>(user_type);
        return enum_type->GetSize();
    }

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
    case MemoryObject::TYPE_STRING:
    case MemoryObject::TYPE_ENUM:
        assert(count <= GetSize());
        memcpy(buf, (void*)data_ptr, count);
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
        ss << "<unk>";
        break;

    case MemoryObject::TYPE_BYTE:
        ss << ".DB";
        break;

    case MemoryObject::TYPE_WORD:
        ss << ".DW";
        break;

    case MemoryObject::TYPE_STRING:
        ss << ".DS";
        break;

    case MemoryObject::TYPE_CODE:
        ss << disassembler->GetInstruction(*data_ptr);

        // For word instructions with an operand address of less than $100, force the word instruction
        if(GetSize() == 3 && data_ptr[2] == 0) ss << ".W";

        break;

    case MemoryObject::TYPE_ENUM: {
        auto enum_type = get<shared_ptr<Enum>>(user_type);
        ss << "enum " << enum_type->GetName();
        break;
    }

    default:
        assert(false);
        break;
    }

    return ss.str();
}

// TODO internal_offset will likely be used later to format multi-line data?
string MemoryObject::FormatOperandField(u32 /* internal_offset */, shared_ptr<Disassembler> disassembler)
{
    stringstream ss;

    if(!backed) { // uninitialized memory has nothing to show and cannot have expressions
        for(int i = 0; i < GetSize(); i++) {
            ss << "?";
        }
    } else {
        // if there's an operand expression, display that, otherwise format a default expression
        if(operand_expression) {
            ss << *operand_expression;
        } else {
            ss << hex << setfill('0') << uppercase;

            switch(type) {
            case MemoryObject::TYPE_UNDEFINED:
            case MemoryObject::TYPE_BYTE:
                ss << "$" << setw(2) << (int)*data_ptr;
                break;

            case MemoryObject::TYPE_WORD: {
                u16 hval = (u16)data_ptr[0] | ((u16)data_ptr[1] << 8);
                ss << "$" << setw(4) << hval;
                break;
            }

            case MemoryObject::TYPE_STRING:
                ss << "\"";
                for(int i = 0; i < string_length; i++) {
                    if(isprint(data_ptr[i])) ss << (char)data_ptr[i];
                    else ss << "\\x" << setw(2) << (int)data_ptr[i];
                }
                ss << "\"";
                break;

            case MemoryObject::TYPE_CODE:
                // this code path is largely not followed
                ss << "<missing expression>";
                break;

            case MemoryObject::TYPE_ENUM: {
                if(enum_element) {
                    ss << enum_element->GetName();
                } else {
                    auto enum_type = get<shared_ptr<Enum>>(user_type);
                    s64 enum_element_value = (s64)data_ptr[0];
                    if(enum_type->GetSize() == 2) enum_element_value |= (s64)((u16)data_ptr[1] << 8);
                    ss << "$" << setw(2*enum_type->GetSize()) << enum_element_value;
                }
                break;
            }

            default:
                assert(false);
                break;
            }
        }
    }

    return ss.str();
}

bool MemoryObject::Save(std::ostream& os, std::string& errmsg)
{
    // save type and if there's data
    WriteVarInt(os, type);
    assert(sizeof(backed) == 1);
    os.write((char*)&backed, sizeof(backed));

    // save string_length for string types
    if(type == MemoryObject::TYPE_STRING) WriteVarInt(os, string_length);

    // save enum type name
    if(type == MemoryObject::TYPE_ENUM) {
        auto enum_type = get<shared_ptr<Enum>>(user_type);
        WriteString(os, enum_type->GetName());
    }

    if(!os.good()) {
        errmsg = "Error writing MemoryObject";
        return false;
    }

    // save only the label strings so we can find them from the system database later
    int nlabels = labels.size();
    WriteVarInt(os, nlabels);
    for(int i = 0; i < nlabels; i++) {
        WriteString(os, labels[i]->GetString());
    }

    // create a fields flag for comments and other bits
    int fields_present = 0;
    fields_present |= (int)(bool)operand_expression << 0;
    fields_present |= (int)(bool)comments.eol       << 1;
    fields_present |= (int)(bool)comments.pre       << 2;
    fields_present |= (int)(bool)comments.post      << 3;
    fields_present |= (int)(!default_blank_line)    << 4;
    WriteVarInt(os, fields_present);

    // operand expression
    if(operand_expression && !operand_expression->Save(os, errmsg)) return false;

    // comments
    if(comments.eol  && !comments.eol->Save(os, errmsg)) return false;
    if(comments.pre  && !comments.pre->Save(os, errmsg)) return false;
    if(comments.post && !comments.post->Save(os, errmsg)) return false;

    // blank line count
    if(!default_blank_line) WriteVarInt(os, blank_lines);

    if(!os.good()) {
        errmsg = "Error writing MemoryObject data";
        return false;
    }

    return true;
}

bool MemoryObject::Load(std::istream& is, std::string& errmsg)
{
    auto system = GetSystem();

    int inttype = ReadVarInt<int>(is);
    type = (MemoryObject::TYPE)inttype;
    is.read((char*)&backed, sizeof(backed));

    //cout << "MemoryObject::type = " << magic_enum::enum_name(type) << endl;
    //cout << "MemoryObject::backed = " << backed << endl;

    // before flat_memory, MemoryObjects saved their data here
    // so we allocate memory of the appropriate size and load it there
    if(GetCurrentProject()->GetSaveFileVersion() < FILE_VERSION_FLATMEMORY) {
        if(backed) {
            u32 size = ReadVarInt<u32>(is);
            if(type == MemoryObject::TYPE_STRING) string_length = (int)size;
            data_ptr = new u8[size];
            is.read((char*)data_ptr, size);
        }
    } else {
        // now we just read string_length for string types
        if(type == MemoryObject::TYPE_STRING) string_length = ReadVarInt<int>(is);
    }

    // TYPE_ENUM didn't exist beforehand so we don't need a special file version for them
    if(type == MemoryObject::TYPE_ENUM) {
        string enum_name;
        ReadString(is, enum_name);
        auto enum_type = system->GetEnum(enum_name);
        if(!enum_type) {
            errmsg = "Enum doesn't exist";
            return false;
        }
        user_type = enum_type;
    }

    int nlabels = ReadVarInt<int>(is);
    //cout << "MemoryObject::nlabels = " << nlabels << endl;
    for(int i = 0; i < nlabels; i++) {
        string label_name;
        ReadString(is, label_name);
        if(!is.good()) {
            errmsg = "Error loading label name";
            return false;
        }

        auto label = system->FindLabel(label_name);
        assert(label);
        if(!label) return false;

        label->SetIndex(i);
        labels.push_back(label);
    }

    int fields_present = ReadVarInt<int>(is);

    if(fields_present & (1 << 0)) {
         operand_expression = make_shared<Expression>();
         if(!operand_expression->Load(is, errmsg)) return false;
    }

    if(fields_present & (1 << 1)) {
        if(GetCurrentProject()->GetSaveFileVersion() < FILE_VERSION_COMMENTS) {
            string s;
            ReadString(is, s);
            comments.eol = make_shared<Comment>();
            comments.eol->Set(s);
        } else {
            if(!(comments.eol = Comment::Load(is, errmsg))) return false;
        }
        //cout << "comment.eol: " << *comments.eol << endl;
    }

    if(fields_present & (1 << 2)) {
        if(GetCurrentProject()->GetSaveFileVersion() < FILE_VERSION_COMMENTS) {
            string s;
            ReadString(is, s);
            comments.pre = make_shared<Comment>();
            comments.pre->Set(s);
        } else {
            if(!(comments.pre = Comment::Load(is, errmsg))) return false;
        }
        //cout << "comment.pre: " << *comments.pre << endl;
    }

    if(fields_present & (1 << 3)) {
        if(GetCurrentProject()->GetSaveFileVersion() < FILE_VERSION_COMMENTS) {
            string s;
            ReadString(is, s);
            comments.post = make_shared<Comment>();
            comments.post->Set(s);
        } else {
            if(!(comments.post = Comment::Load(is, errmsg))) return false;
        }
        //cout << "comment.post: " << *comments.post << endl;
    }

    if(fields_present & (1 << 4)) {
        blank_lines = ReadVarInt<int>(is);
        default_blank_line = false;
    }

    return true;
}

bool MemoryRegion::Save(std::ostream& os, std::string& errmsg)
{
    // save name
    WriteString(os, name);

    // save base and size
    WriteVarInt(os, base_address);
    WriteVarInt(os, region_size);
    if(!os.good()) {
        errmsg = "Error writing data";
        return false;
    }

    // save the flat memory here
    WriteVarInt(os, (int)(bool)(flat_memory));
    if(flat_memory) os.write((char*)flat_memory, region_size);

    // save all the unique memory objects
    for(u32 offset = 0; offset < region_size; ) {
        auto memory_object = object_refs[offset];
        if(!memory_object->Save(os, errmsg)) return false;
        offset += memory_object->GetSize(); // skip addresses that point to the same object
    }

    return true;
}

bool MemoryRegion::Load(GlobalMemoryLocation const& base, std::istream& is, std::string& errmsg)
{
    GlobalMemoryLocation where(base);

    // save name
    ReadString(is, name);
    if(!is.good()) return false;

    base_address = ReadVarInt<u32>(is);
    region_size = ReadVarInt<u32>(is);
    if(!is.good()) {
        errmsg = "Error reading region address";
        return false;
    }
    //cout << "MemoryRegion::base_address = " << hex << base_address << endl;
    //cout << "MemoryRegion::region_size = " << hex << region_size << endl;

    // flat_memory is stored here. before FILE_VERSION_FLATMEMORY, we can't yet
    // tell if our memory is backed, so we have to wait until objects are loaded
    if(GetCurrentProject()->GetSaveFileVersion() >= FILE_VERSION_FLATMEMORY) {
        bool backed = (bool)ReadVarInt<int>(is);
        if(backed) {
            flat_memory = new u8[region_size];
            is.read((char*)flat_memory, region_size);
        }
    }

    // initialize memory object storage
    Erase();
    object_refs.resize(region_size);

    // load all the memory objects
    for(u32 offset = 0; offset < region_size;) {
        where.address = base_address + offset;

        auto obj = make_shared<MemoryObject>();
        if(!obj->Load(is, errmsg)) return false;

        // old projects stored their data in the memory object, so we need to copy 
        // that over to flat_memory.
        if(GetCurrentProject()->GetSaveFileVersion() < FILE_VERSION_FLATMEMORY) {
            if(obj->backed) {
                // allocate memory when we encounter the first backed object
                if(!flat_memory) flat_memory = new u8[region_size];

                // copy the data
                memcpy(&flat_memory[offset], obj->data_ptr, obj->GetSize());

                // free the objects pointer
                delete [] obj->data_ptr;
                obj->data_ptr = nullptr;
            }
        }

        // set data_ptr here so that GetSize() works correctly
        if(obj->backed) obj->data_ptr = &flat_memory[offset];

        // set all the comments to their location
        if(obj->comments.pre) {
            auto com = dynamic_pointer_cast<Comment>(obj->comments.pre);
            com->SetLocation(where);
        }
        if(obj->comments.eol) {
            auto com = dynamic_pointer_cast<Comment>(obj->comments.eol);
            com->SetLocation(where);
        }
        if(obj->comments.post) {
            auto com = dynamic_pointer_cast<Comment>(obj->comments.post);
            com->SetLocation(where);
        }

        // we used to call obj->NoteReference() here, but we need all memory locations to be loaded
        // (and therefore assigned all their labels) before we can note any references. 
        // It's a problem if there's a label reference at $8000 referring to $9000, but memory object $9000
        // hasn't been loaded yet. So after ALL memory has been loaded, NoteReferences() is called

        // set all memory locations offset..offset+size-1 to the object
        for(u32 i = 0; i < obj->GetSize(); i++) object_refs[offset + i] = obj;

        // next offset
        auto obj_offset = offset;
        offset += obj->GetSize();

     }

    // Rebuild the object tree using the list of object references
    ReinitializeFromObjectRefs();

    return true;
}

void MemoryRegion::SetComment(GlobalMemoryLocation const& where, MemoryObject::COMMENT_TYPE type, 
                shared_ptr<BaseComment> const& comment) {
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->SetComment(type, comment);
        // TODO when GlobalMemoryLocation is no longer part of Systems::NES
        // then we don't need the cast anymore and SetLocation can be part of BaseComment
        auto nes_comment = dynamic_pointer_cast<Comment>(comment);
        nes_comment->SetLocation(where);
        UpdateMemoryObject(where);
    }
}

void MemoryRegion::AddBlankLine(GlobalMemoryLocation const& where)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->blank_lines++;
        memory_object->default_blank_line = false;
        UpdateMemoryObject(where);
    }
}

void MemoryRegion::RemoveBlankLine(GlobalMemoryLocation const& where)
{
    if(auto memory_object = GetMemoryObject(where)) {
        if(memory_object->blank_lines > 0) {
            memory_object->blank_lines--;
            memory_object->default_blank_line = false;

            if(memory_object->blank_lines == 0 && memory_object->labels.size() == 0) {
                memory_object->default_blank_line = true;
            }
        }
        UpdateMemoryObject(where);
    }
}

void MemoryRegion::NoteReferences(GlobalMemoryLocation const& base)
{
    GlobalMemoryLocation where(base);

    for(u32 offset = 0; offset < region_size;) {
        where.address = base_address + offset;

        auto memory_object = GetMemoryObject(where);
        memory_object->NoteReferences(where);

        offset += memory_object->GetSize();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProgramRomBank::ProgramRomBank(shared_ptr<System>& system, int _prg_rom_bank, string const& name, PROGRAM_ROM_BANK_LOAD _bank_load, PROGRAM_ROM_BANK_SIZE _bank_size) 
    : MemoryRegion(system, name), prg_rom_bank(_prg_rom_bank), bank_load(_bank_load), bank_size(_bank_size) 
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

bool ProgramRomBank::GetGlobalMemoryLocation(u32 offset, GlobalMemoryLocation* out)
{
    if(!MemoryRegion::GetGlobalMemoryLocation(offset, out)) return false;
    out->is_chr = false;
    out->prg_rom_bank = prg_rom_bank;
    return true;
}

bool ProgramRomBank::Save(std::ostream& os, std::string& errmsg)
{
    WriteVarInt(os, prg_rom_bank);
    WriteVarInt(os, bank_load);
    WriteVarInt(os, bank_size);
    if(!os.good()) {
        errmsg = "Error writing data";
        return false;
    }

    return MemoryRegion::Save(os, errmsg);
}

shared_ptr<ProgramRomBank> ProgramRomBank::Load(std::istream& is, std::string& errmsg, shared_ptr<System>& system)
{
    int prg_rom_bank = ReadVarInt<int>(is);
    PROGRAM_ROM_BANK_LOAD bank_load = (PROGRAM_ROM_BANK_LOAD)ReadVarInt<int>(is);
    PROGRAM_ROM_BANK_SIZE bank_size = (PROGRAM_ROM_BANK_SIZE)ReadVarInt<int>(is);
    if(!is.good()) return nullptr;
    auto prg_bank = make_shared<ProgramRomBank>(system, prg_rom_bank, "", bank_load, bank_size);
    GlobalMemoryLocation base {
        .address = (u16)prg_bank->GetBaseAddress(),
        .is_chr = false,
        .prg_rom_bank = (u16)prg_rom_bank,
    };
    if(!prg_bank->MemoryRegion::Load(base, is, errmsg)) return nullptr;
    return prg_bank;
}

void ProgramRomBank::NoteReferences()
{
    GlobalMemoryLocation base {
        .is_chr = false,
        .prg_rom_bank = (u16)prg_rom_bank
    };

    MemoryRegion::NoteReferences(base);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CharacterRomBank::CharacterRomBank(shared_ptr<System>& system, int _chr_rom_bank, string const& name, CHARACTER_ROM_BANK_LOAD _bank_load, CHARACTER_ROM_BANK_SIZE _bank_size)
    : MemoryRegion(system, name), chr_rom_bank(_chr_rom_bank), bank_load(_bank_load), bank_size(_bank_size)
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

bool CharacterRomBank::GetGlobalMemoryLocation(u32 offset, GlobalMemoryLocation* out)
{
    if(!MemoryRegion::GetGlobalMemoryLocation(offset, out)) return false;
    out->is_chr = true;
    out->chr_rom_bank = chr_rom_bank;
    return true;
}

bool CharacterRomBank::Save(std::ostream& os, std::string& errmsg)
{
    WriteVarInt(os, chr_rom_bank);
    WriteVarInt(os, bank_load);
    WriteVarInt(os, bank_size);
    if(!os.good()) {
        errmsg = "Error writing data";
        return false;
    }

    return MemoryRegion::Save(os, errmsg);
}

shared_ptr<CharacterRomBank> CharacterRomBank::Load(std::istream& is, std::string& errmsg, shared_ptr<System>& system)
{
    int chr_rom_bank = ReadVarInt<int>(is);
    CHARACTER_ROM_BANK_LOAD bank_load = (CHARACTER_ROM_BANK_LOAD)ReadVarInt<int>(is);
    CHARACTER_ROM_BANK_SIZE bank_size = (CHARACTER_ROM_BANK_SIZE)ReadVarInt<int>(is);
    if(!is.good()) return nullptr;
    auto chr_bank = make_shared<CharacterRomBank>(system, chr_rom_bank, "", bank_load, bank_size);
    GlobalMemoryLocation base {
        .address = (u16)chr_bank->GetBaseAddress(),
        .is_chr = true,
        .chr_rom_bank = (u16)chr_rom_bank,
    };
    if(!chr_bank->MemoryRegion::Load(base, is, errmsg)) return nullptr;
    return chr_bank;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generic RAM
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RAMRegion::RAMRegion(shared_ptr<System>& system, string const& name, u32 _base_adress, u32 _region_size)
    : MemoryRegion(system, name)
{
    base_address = _base_adress;
    region_size = _region_size;
}

bool RAMRegion::Load(istream& is, string& errmsg)
{
    GlobalMemoryLocation base {
        .address = (u16)base_address,
        .is_chr  = false,
    };

    return MemoryRegion::Load(base, is, errmsg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PPU registers $2000-$2008 (mirroed every 8 bytes until 0x3FFF)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
PPURegistersRegion::PPURegistersRegion(shared_ptr<System>& system)
    : MemoryRegion(system, "PPUREGS")
{
    base_address = 0x2000;
    region_size  = 0x2000;
}

bool PPURegistersRegion::Load(istream& is, string& errmsg)
{
    GlobalMemoryLocation base {
        .address = (u16)base_address,
        .is_chr  = false,
    };

    return MemoryRegion::Load(base, is, errmsg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// APU and I/O registers $4000-$401F (doesn't have mirrored data)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IORegistersRegion::IORegistersRegion(shared_ptr<System>& system)
    : MemoryRegion(system, "IOREGS")
{
    base_address = 0x4000;
    region_size  = 0x20;
}

bool IORegistersRegion::Load(istream& is, string& errmsg)
{
    GlobalMemoryLocation base {
        .address = (u16)base_address,
        .is_chr  = false,
    };

    return MemoryRegion::Load(base, is, errmsg);
}

}
