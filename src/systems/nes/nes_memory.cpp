#include <cassert>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>

#include "imgui.h"
#include "imgui_internal.h"

#include "main.h"
#include "magic_enum.hpp"

#include "systems/nes/nes_disasm.h"
#include "systems/nes/nes_expressions.h"
#include "systems/nes/nes_label.h"
#include "systems/nes/nes_listing.h"
#include "systems/nes/nes_project.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

bool GlobalMemoryLocation::Save(std::ostream& os, std::string& errmsg)
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

    // create a blank line inbetween other memory and labels, unless at the start of the bank
    // TODO or if it's a local label
    if(obj->labels.size() && region_offset != 0) {
        obj->listing_items.push_back(make_shared<ListingItemBlankLine>());
    }

    for(int nth = 0; nth < obj->labels.size(); nth++) {
        auto& label = obj->labels[nth];
        obj->listing_items.push_back(make_shared<ListingItemLabel>(label, nth));
    }

    // create the pre comment
    if(obj->comments.pre) obj->listing_items.push_back(make_shared<ListingItemPrePostComment>(0, false));

    // the primary index is used to focus on code or data when moving to locations in the listing windows
    obj->primary_listing_item_index = obj->listing_items.size();
    obj->listing_items.push_back(make_shared<ListingItemPrimary>(0));

    // create the post comment
    if(obj->comments.post) obj->listing_items.push_back(make_shared<ListingItemPrePostComment>(0, true));
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
        obj->backed = true;
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

// I don't like the duplicated code between this and _InitializeFromData
void MemoryRegion::_InitializeEmpty(shared_ptr<MemoryObjectTreeNode>& tree_node, u32 region_offset, int count)
{
    // stop the iteration when there's one byte left
    if(count == 1) {
        tree_node->is_object = true;

        // create the object
        shared_ptr<MemoryObject> obj = make_shared<MemoryObject>();
        obj->parent = tree_node;

        // set the data
        obj->type = MemoryObject::TYPE_UNDEFINED;
        obj->backed = false;

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

        // just set the parent
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

        // Don't convert already dead objects
        if(memory_object->type == MemoryObject::TYPE_UNDEFINED) {
            offset += memory_object->GetSize();
            continue;
        }

        int size = memory_object->GetSize();

        // store the memory's actual raw data
        u8* tmp = (u8*)_alloca(size);
        memory_object->Read(tmp, size);

        // save the is_object tree node before clearing memory_object from the tree
        auto tree_node = memory_object->parent.lock();

        // save this objects labels
        auto& labels = memory_object->labels;

        // clear any references this object is making
        memory_object->ClearReferences(where + offset);

        // remove memory_object from the tree first, this will correct listing item counts
        RemoveMemoryObjectFromTree(memory_object, true);

        // clear the is_object status of the tree node and build a tree with the data under it
        // this will update the object_refs[] array
        tree_node->is_object = false;
        u32 region_offset = ConvertToRegionOffset(where.address + offset);
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

        // move past this object
        offset += size;
    }

    // the old memory_object will go out of scope here
    return true;
}

bool MemoryRegion::MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Round up
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
                cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << "+1 cannot be converted to a word (currently type " << magic_enum::enum_name(next_object->type) << ")" << endl;
                return false;
            }

            break;
        }

        default:
            cout << "[MemoryRegion::MarkMemoryAsWords] address 0x" << (where.address + i) << " cannot be converted to a word (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
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
            cout << "[MemoryRegion::MarkMemoryAsCode] address " << (where + i) << " cannot be converted to code (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
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

        // steal data from operand_object
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

bool MemoryRegion::MarkMemoryAsString(GlobalMemoryLocation const& where, u32 byte_count)
{
    // Check to see if all selected memory can be converted
    for(u32 i = 0; i < byte_count; i++) {
        auto memory_object = GetMemoryObject(where + i);
        if(memory_object->type != MemoryObject::TYPE_BYTE && memory_object->type != MemoryObject::TYPE_UNDEFINED) {
            cout << "[MemoryRegion::MarkMemoryAsString] address " << (where + i) << " cannot be converted to code (currently type " << magic_enum::enum_name(memory_object->type) << ")" << endl;
            return false;
        }
    }

    // The first object will be changed into the string
    auto str_object = GetMemoryObject(where);

    // allocate the storage for the data
    u8 first_byte = str_object->bval;
    str_object->str.data = new u8[byte_count];
    str_object->str.data[0] = first_byte;
    str_object->str.len = byte_count;

    // get the rest of the string bytes and remove them from the tree, adding them to the string object
    for(u32 i = 1; i < byte_count; i++) {
        auto next_byte_object = GetMemoryObject(where + i);
        assert(next_byte_object->type == MemoryObject::TYPE_BYTE || next_byte_object->type == MemoryObject::TYPE_UNDEFINED);
        RemoveMemoryObjectFromTree(next_byte_object);

        // steal data from next_byte_object
        str_object->str.data[i] = next_byte_object->bval;

        // update the object_refs
        u32 x = ConvertToRegionOffset((where + i).address);
        object_refs[x] = str_object;
    }

    // convert the str_object to TYPE_STRING and update the tree
    str_object->type = MemoryObject::TYPE_STRING;
    UpdateMemoryObject(where);

    return true;
}

void MemoryRegion::SetOperandExpression(GlobalMemoryLocation const& where, std::shared_ptr<Expression> const& expr)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->ClearReferences(where); // clear any references the previous operand expression referred to
        memory_object->operand_expression = expr;
        memory_object->SetReferences(where);   // mark the new ones
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

void MemoryRegion::ClearReferencesToLabels(GlobalMemoryLocation const& where)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->ClearReferences(where); // clear all references first
        memory_object->ClearReferencesToLabels(where);
        memory_object->SetReferences(where);   // re-set all references (i.e., define)
    }
}

void MemoryRegion::NextLabelReference(GlobalMemoryLocation const& where)
{
    if(auto memory_object = GetMemoryObject(where)) {
        memory_object->ClearReferences(where); // clear all references first
        memory_object->NextLabelReference(where);
        memory_object->SetReferences(where);   // re-set all references (i.e., define)
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

struct MemoryObject::LabelCreatedData {
    GlobalMemoryLocation target;
    signal_connection connection;
};

void MemoryObject::SetReferences(GlobalMemoryLocation const& where)
{
    // if there's no operand expresion, there are no references
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // Explore operand_expression and mark each referenced Define() that we're referring to it
    auto cb = [this, &where](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->NoteReference(where);
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // tell the expression node to update the reference to the label
            label_node->NoteReference(where);

            // and create a callback for any label created at the target address
            auto system = MyApp::Instance()->GetProject()->GetSystem<System>();
            GlobalMemoryLocation const& target = label_node->GetTarget();

            label_connections.push_back(make_shared<LabelCreatedData>(LabelCreatedData {
                .target = target,
                .connection = system->LabelCreatedAt(target)->connect(
                        [this, where, label_node](shared_ptr<Label> const& label, bool was_user_created) {
                            // this will notify the new label that we're referring to it. if a different label is created
                            // at the same address, this won't reference that label since the current expression node already has
                            // a label
                            label_node->NoteReference(where);
                        })
            }));
        }
        return true;
    };

    // TODO clear all label_created signal handlers before recreating them
    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

void MemoryObject::ClearReferences(GlobalMemoryLocation const& where)
{
    auto system = MyApp::Instance()->GetProject()->GetSystem<System>();

    // Clear all the label_created signal connections
    for(auto& data : label_connections) {
        data->connection->disconnect();
        system->LabelCreatedAtRemoved(data->target);
    }

    label_connections.clear();

    // if there's no operand expresion, there are no references
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // Explore operand_expression and tell each referenced object we no longer care about them
    auto cb = [&where](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto define_node = dynamic_pointer_cast<ExpressionNodes::Define>(node)) {
            define_node->GetDefine()->RemoveReference(where);
        } else if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            label_node->RemoveReference(where);
        }
        return true;
    };

    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

void MemoryObject::ClearReferencesToLabels(GlobalMemoryLocation const& where)
{
    // if there's no operand expresion, there are no references
    if(!operand_expression || !operand_expression->GetRoot()) return;

    auto nc = dynamic_pointer_cast<ExpressionNodeCreator>(operand_expression->GetNodeCreator());

    // Explore the expression, changing labels to constants
    auto cb = [&where, &nc](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // evaluate the label to its address
            s64 address;
            string errmsg;
            bool result = label_node->Evaluate(&address, errmsg);
            assert(result);

            // convert the label_node to a constant node
            node = nc->CreateConstant(address, label_node->GetDisplay());
        }
        return true;
    };

    if(!operand_expression->Explore(cb, nullptr)) assert(false); // false return shouldn't happen
}

// Change to the next label at a given address
void MemoryObject::NextLabelReference(GlobalMemoryLocation const& where)
{
    // if there's no operand expresion, there are no labels
    if(!operand_expression || !operand_expression->GetRoot()) return;

    // Explore the expression, calling Increment on the first and then bailing
    auto cb = [&where](shared_ptr<BaseExpressionNode>& node, shared_ptr<BaseExpressionNode> const&, int, void*)->bool {
        if(auto label_node = dynamic_pointer_cast<ExpressionNodes::Label>(node)) {
            // convert the label_node to a constant node
            label_node->NextLabel();
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
        return disassembler->GetInstructionSize(code.opcode);

    case MemoryObject::TYPE_STRING:
        return str.len;

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

    case MemoryObject::TYPE_STRING:
        memcpy(buf, str.data, count);
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

    case MemoryObject::TYPE_STRING:
        ss << ".DS";
        break;

    case MemoryObject::TYPE_CODE:
        ss << disassembler->GetInstruction(code.opcode);

        // For word instructions with an operand address of less than $100, force the word instruction
        if(GetSize() == 3 && code.operands[1] == 0) ss << ".W";

        break;

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
                ss << "$" << setw(2) << (int)bval;
                break;

            case MemoryObject::TYPE_WORD:
                ss << "$" << setw(4) << hval;
                break;

            case MemoryObject::TYPE_STRING:
                ss << "\"";
                for(int i = 0; i < GetSize(); i++) {
                    if(isprint(str.data[i])) ss << (char)str.data[i];
                    else ss << "\\x" << setw(2) << (int)str.data[i];
                }
                ss << "\"";
                break;

            case MemoryObject::TYPE_CODE:
                // this code path is largely not followed
                ss << "<missing expression>";
                break;

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

    // save the data (for now, TODO: don't save data and instead read from the rom file?)
    if(backed) {
        WriteVarInt(os, GetSize());
        if(type == MemoryObject::TYPE_STRING) {
            os.write((char*)str.data, GetSize());
        } else {
            os.write((char*)&bval, GetSize());
        }
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
    WriteVarInt(os, fields_present);

    // operand expression
    if(operand_expression && !operand_expression->Save(os, errmsg)) return false;

    // comments
    if(comments.eol)  WriteString(os, *comments.eol);
    if(comments.pre)  WriteString(os, *comments.pre);
    if(comments.post) WriteString(os, *comments.post);

    if(!os.good()) {
        errmsg = "Error writing MemoryObject data";
        return false;
    }

    return true;
}

bool MemoryObject::Load(std::istream& is, std::string& errmsg)
{
    auto system = MyApp::Instance()->GetProject()->GetSystem<System>();

    int inttype = ReadVarInt<int>(is);
    type = (MemoryObject::TYPE)inttype;
    is.read((char*)&backed, sizeof(backed));

    //cout << "MemoryObject::type = " << magic_enum::enum_name(type) << endl;
    //cout << "MemoryObject::backed = " << backed << endl;

    if(backed) {
        u32 size = ReadVarInt<u32>(is);
        //cout << "MemoryObject::size = " << size << endl;
        if(type == MemoryObject::TYPE_STRING) {
            str.data = new u8[size];
            str.len = size;
            is.read((char*)str.data, str.len);
        } else {
            is.read((char*)&bval, size);
        }
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
        string s;
        ReadString(is, s);
        comments.eol = make_shared<string>(s);
        cout << "comment.eol: " << *comments.eol << endl;
    }

    if(fields_present & (1 << 2)) {
        string s;
        ReadString(is, s);
        comments.pre = make_shared<string>(s);
        cout << "comment.pre: " << *comments.pre << endl;
    }

    if(fields_present & (1 << 3)) {
        string s;
        ReadString(is, s);
        comments.post = make_shared<string>(s);
        cout << "comment.post: " << *comments.post << endl;
    }

    return true;
}

u8 MemoryRegion::ReadByte(GlobalMemoryLocation const& where)
{
    return 0;
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

    // initialize memory object storage
    Erase();
    object_refs.resize(region_size);

    // load all the memory objects
    for(u32 offset = 0; offset < region_size;) {
        where.address = base_address + offset;

        auto obj = make_shared<MemoryObject>();
        if(!obj->Load(is, errmsg)) return false;
        
        // put labels in the systems's label database
        if(auto system = parent_system.lock()) {
            for(auto& label : obj->labels) {
                system->InsertLabel(label);
            }
        }

        // update label/define references
        obj->SetReferences(where);

        // set all memory locations offset..offset+size-1 to the object
        for(u32 i = 0; i < obj->GetSize(); i++) object_refs[offset + i] = obj;

        // next offset
        offset += obj->GetSize();
    }

    // Rebuild the object tree using the list of object references
    ReinitializeFromObjectRefs();

    return true;
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
// CPU RAM $0000-$0800 (mirroed every $800 bytes)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RAMRegion::RAMRegion(shared_ptr<System>& system)
    : MemoryRegion(system, "RAM")
{
    base_address = 0x0000;
    region_size  = 0x0800;
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
