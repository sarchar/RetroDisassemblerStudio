// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <cassert>
#include <iostream>
#include <iomanip>
#include <vector>

#include "util.h"

#include "systems/nes/defs.h"

namespace Windows::NES {
    class ListingItem;
}

namespace Systems::NES {

class Disassembler;
class Expression;
class Label;
class System;

// SystemMemoryLocation dials into a specific byte within the system. It has enough information to select which
// segment of the system (RAM, SRAM, etc) as well as which ROM bank, overlay or any psuedo location that may exist.
struct GlobalMemoryLocation {
    // 16-bit address space
    u16 address = 0;

    // set to true if we're reading CHR-RAM
    bool is_chr = false;

    // used only for PRG
    u16 prg_rom_bank = 0;

    // used only for CHR
    u16 chr_rom_bank = 0;

    template <class T>
    void Increment(T const& v) {
        address += v; // TODO wrap, increment banks, etc
    }

    template<typename T>
    GlobalMemoryLocation operator+(T const& v) const {
        GlobalMemoryLocation ret(*this);
        ret.Increment(v);
        return ret;
    }

    GlobalMemoryLocation& operator++() {
        Increment(1);
        return *this;
    }

    bool Save(std::ostream&, std::string&) const;
    bool Load(std::istream&, std::string&);

    bool operator==(GlobalMemoryLocation const& other) const {
        return address == other.address &&
            (is_chr ? ( other.is_chr && chr_rom_bank == other.chr_rom_bank)
                    : (!other.is_chr && prg_rom_bank == other.prg_rom_bank)
            );
    }

    bool operator<(GlobalMemoryLocation const& other) const {
        return (!is_chr && other.is_chr) 
            || (!is_chr && (prg_rom_bank < other.prg_rom_bank || address < other.address))
            || (is_chr && (chr_rom_bank < other.chr_rom_bank || address < other.address));
    }

    struct HashFunction {
        size_t operator()(GlobalMemoryLocation const& other) const {
            u64 x = (u64)other.address;
            x |= ((u64)other.is_chr)       << 16;
            x |= ((u64)other.prg_rom_bank) << 32;
            x |= ((u64)other.chr_rom_bank) << 48;
            return std::hash<u64>()(x);
        }
    };

    friend std::ostream& operator<<(std::ostream& stream, const GlobalMemoryLocation& p) {
        std::ios_base::fmtflags saveflags(stream.flags());
        stream << "GlobalMemoryLocation(address=0x" << std::hex << std::setw(4) << std::setfill('0') << std::uppercase << p.address;
        stream << ", prg_rom_bank=" << std::dec << std::setw(0) << p.prg_rom_bank;
        stream << ", chr_rom_bank=" << std::dec << std::setw(0) << p.chr_rom_bank;
        stream << ", is_chr=" << p.is_chr;
        stream << ")";
        stream.flags(saveflags);
        return stream;
    }

    void FormatAddress(std::ostream& stream, bool force_16 = false, bool show_bank = true, bool with_colon = true) const {
        std::ios_base::fmtflags saveflags(stream.flags());
        stream << "$" << std::hex << std::uppercase << std::setfill('0');
        if(show_bank) {
            stream << std::setw(2) << (is_chr ? chr_rom_bank : prg_rom_bank);
            if(with_colon) stream << ":";
        }
        if(force_16 || address >= 0x100) stream << std::setw(4);
        else stream << std::setw(2);
        stream << address;
        stream.flags(saveflags);
    }

};

class MemoryObject;
class MemoryRegion;

// The job of the tree is to keep memory objects ordered, provide iterators over objects
// and keep track of listings
struct MemoryObjectTreeNode {
    using ListingItem = Windows::NES::ListingItem;

    std::weak_ptr<MemoryObjectTreeNode>   parent;
    std::shared_ptr<MemoryObjectTreeNode> left  = nullptr;
    std::shared_ptr<MemoryObjectTreeNode> right = nullptr;
    std::shared_ptr<MemoryObject>         obj   = nullptr;

    // sum of all listing items in the left, right, and obj pointers
    u32 listing_item_count = 0;

    // when is_object is set, left and right are not valid and obj is
    bool is_object = false;

    MemoryObjectTreeNode(std::shared_ptr<MemoryObjectTreeNode> p)
        : parent(p) {}

    struct iterator {
        std::shared_ptr<Disassembler> disassembler;
        std::shared_ptr<MemoryRegion> memory_region;
        std::shared_ptr<MemoryObject> memory_object;
        u32 listing_item_index;                           
        u32 region_offset;
        
        std::shared_ptr<ListingItem>& GetListingItem();
        u32 GetCurrentAddress();
        u32 GetListingItemIndex() const { return listing_item_index; }

        iterator& operator++();
    };
};

struct MemoryObject {
    using ListingItem = Windows::NES::ListingItem;

    enum TYPE {
        TYPE_UNDEFINED, // for NES undefined data shows up as bytes
        TYPE_BYTE,
        TYPE_WORD,
        TYPE_CODE,
        TYPE_STRING,

        // Even though LIST and ARRAY function interally very similarly,
        // a LIST type is internal to the editor (it's a list of memory objects), whereas
        // the ARRAY type is indexable in code statements.
        TYPE_LIST,
        TYPE_ARRAY
    };

    enum COMMENT_TYPE {
        COMMENT_TYPE_EOL,
        COMMENT_TYPE_PRE,
        COMMENT_TYPE_POST,
    };

    TYPE type = TYPE_UNDEFINED;
    bool backed = false; // false if the data is uninitialized memory

    std::weak_ptr<MemoryObjectTreeNode> parent;

    std::vector<std::shared_ptr<Label>> labels;
    std::vector<std::shared_ptr<ListingItem>> listing_items;
    int primary_listing_item_index;

    std::shared_ptr<Expression> operand_expression;

    struct {
        std::shared_ptr<std::string> eol;
        std::shared_ptr<std::string> pre;
        std::shared_ptr<std::string> post;
    } comments;

    union {
        u8  bval;
        u16 hval;

        struct {
            u8* data;
            int len;
        } str;

        struct {
            u8 opcode;
            u8 operands[2]; // max 2 per opcode
        } code;

        // TODO array
        std::vector<std::shared_ptr<MemoryObject>> array = {};
    };

    MemoryObject() {}
    ~MemoryObject() {
        if(type == TYPE_STRING) delete [] str.data;
    }

    void NoteReferences(GlobalMemoryLocation const&);
    void RemoveReferences(GlobalMemoryLocation const&);

    u32 GetSize(std::shared_ptr<Disassembler> disassembler = nullptr);
    void Read(u8*, int);

    std::string FormatInstructionField(std::shared_ptr<Disassembler> disassembler = nullptr);
    std::string FormatOperandField(u32 = 0, std::shared_ptr<Disassembler> disassembler = nullptr);

    void GetComment(COMMENT_TYPE type, std::string& out) {
        out.clear();

        switch(type) {
        case COMMENT_TYPE_EOL:
            if(comments.eol) out = std::string(*comments.eol);
            break;
        case COMMENT_TYPE_PRE:
            if(comments.pre) out = std::string(*comments.pre);
            break;
        case COMMENT_TYPE_POST:
            if(comments.post) out = std::string(*comments.post);
            break;
        }
    }

    struct LabelCreatedData;
    std::vector<std::shared_ptr<LabelCreatedData>> label_connections; // usually empty, rarely contains more than 1

    bool Save(std::ostream&, std::string&);
    bool Load(std::istream&, std::string&);

private:
    void ClearReferencesToLabels(GlobalMemoryLocation const& where);
    void NextLabelReference(GlobalMemoryLocation const& where);
    int  DeleteLabel(std::shared_ptr<Label> const&); // call MemoryRegion::DeleteLabel

    void SetComment(COMMENT_TYPE type, std::string const& comment) {
        switch(type) {
        case COMMENT_TYPE_EOL:
            comments.eol = std::make_shared<std::string>(comment);
            break;
        case COMMENT_TYPE_PRE:
            comments.pre = std::make_shared<std::string>(comment);
            break;
        case COMMENT_TYPE_POST:
            comments.post = std::make_shared<std::string>(comment);
            break;
        default:
            assert(false);
            break;
        }
    }

    friend class MemoryRegion;
};

// MemoryRegion represents a region of memory on the system
// Memory regions are a list of content ordered by the contents offset in the block
// But because lookups would be slow with blocks of content, we still have a pointer into the content table for each address in the region
class MemoryRegion : public std::enable_shared_from_this<MemoryRegion> {
public:
    typedef std::vector<std::shared_ptr<MemoryObject>> ObjectRefListType;

    MemoryRegion(std::shared_ptr<System>&, std::string const&);
    ~MemoryRegion();

    std::string const& GetName() const { return name; }
    u32 GetBaseAddress()         const { return base_address; }
    u32 GetRegionSize()          const { return region_size; }
    u32 GetEndAddress()          const { return base_address + region_size; }
    u32 GetTotalListingItems()   const { return object_tree_root ? object_tree_root->listing_item_count : 0; }

    inline u32 ConvertToRegionOffset(u32 address_in_region) { 
        assert(address_in_region >= base_address && address_in_region < base_address + region_size);
        return address_in_region - base_address; 
    }

    void                           InitializeEmpty();
    void                           InitializeFromData(u8* data, int count);
    void                           ReinitializeFromObjectRefs();

    std::shared_ptr<MemoryObject>  GetMemoryObject(GlobalMemoryLocation const&, int* offset = NULL);
    void                           UpdateMemoryObject(GlobalMemoryLocation const&);

    MemoryObject::TYPE             GetMemoryObjectType(GlobalMemoryLocation const& where) { return GetMemoryObject(where)->type; }

    virtual bool                   GetGlobalMemoryLocation(u32, GlobalMemoryLocation*);

    // Listing help
    std::shared_ptr<MemoryObjectTreeNode::iterator> GetListingItemIterator(int listing_item_start_index);

    u32  GetListingIndexByAddress(GlobalMemoryLocation const&);

    u8  ReadByte(int offset);
    void Copy(u8* dest, int offset, int size);

    // Labels
    void ApplyLabel(std::shared_ptr<Label>&);
    int  DeleteLabel(std::shared_ptr<Label> const&);
    void ClearReferencesToLabels(GlobalMemoryLocation const& where);
    void NextLabelReference(GlobalMemoryLocation const& where);

    // Data
    bool MarkMemoryAsUndefined(GlobalMemoryLocation const& where, u32 byte_count);
    bool MarkMemoryAsWords(GlobalMemoryLocation const& where, u32 byte_count);
    bool MarkMemoryAsString(GlobalMemoryLocation const& where, u32 byte_count);

    // Code
    bool MarkMemoryAsCode(GlobalMemoryLocation const& where, u32 byte_count);
    void SetOperandExpression(GlobalMemoryLocation const& where, std::shared_ptr<Expression> const&);

    // Comments
    void GetComment(GlobalMemoryLocation const& where, MemoryObject::COMMENT_TYPE type, std::string& out) {
        if(auto memory_object = GetMemoryObject(where)) {
            memory_object->GetComment(type, out);
        }
    }

    void SetComment(GlobalMemoryLocation const& where, MemoryObject::COMMENT_TYPE type, std::string const& s) {
        if(auto memory_object = GetMemoryObject(where)) {
            memory_object->SetComment(type, s);
            UpdateMemoryObject(where);
        }
    }

    // References
    void NoteReferences(GlobalMemoryLocation const&);

    // Load and save
    virtual bool Save(std::ostream&, std::string&);
    virtual bool Load(GlobalMemoryLocation const&, std::istream&, std::string&);

protected:
    u32 base_address;
    u32 region_size;
    std::weak_ptr<System> parent_system;

private:
    std::string name;

    void Erase();

    // We need a list of all memory addresses pointing to objects
    // This is initialized to byte objects for each address the memory is initialized with
    ObjectRefListType object_refs;

    // And we need the Root of the object tree
    std::shared_ptr<MemoryObjectTreeNode> object_tree_root = nullptr;

    void _InitializeEmpty(std::shared_ptr<MemoryObjectTreeNode>&, u32, int);
    void _InitializeFromData(std::shared_ptr<MemoryObjectTreeNode>&, u32, u8*, int);
    void _ReinializeFromObjectRefs(std::shared_ptr<MemoryObjectTreeNode>&, std::vector<int> const&, u32, int);
    void _UpdateMemoryObject(std::shared_ptr<MemoryObject>&, u32);
    void RemoveMemoryObjectFromTree(std::shared_ptr<MemoryObject>&, bool save_tree_node = false);

    void RecalculateListingItemCounts();
    void _SumListingItemCountsUp(std::shared_ptr<MemoryObjectTreeNode>&);
    void _RecalculateListingItemCounts(std::shared_ptr<MemoryObjectTreeNode>&);
    void RecreateListingItems();
    void RecreateListingItemsForMemoryObject(std::shared_ptr<MemoryObject>&, u32);

    // Utility for GetListingItemIterator to find region offsets based on listing item index
    u32 FindRegionOffsetForListingItem(int);
    u32 GetListingItemIndexForMemoryObject(std::shared_ptr<MemoryObject> const&);

    // during emulation, we will want a cache for already translated code
    // u8 opcode_cache[];
    // and probably a bitmap indicating whether an address is valid in the cache
    // u64 opcode_cache_valid[16 * 1024 / 64] // one bit per address using 64 bit ints
};

class ProgramRomBank : public MemoryRegion {
public:
    ProgramRomBank(std::shared_ptr<System>&, int, std::string const&, PROGRAM_ROM_BANK_LOAD, PROGRAM_ROM_BANK_SIZE);
    virtual ~ProgramRomBank() {};

    bool GetGlobalMemoryLocation(u32, GlobalMemoryLocation*) override;

    void NoteReferences();

    bool Save(std::ostream&, std::string&) override;
    static std::shared_ptr<ProgramRomBank> Load(std::istream&, std::string&, std::shared_ptr<System>&);
private:
    int prg_rom_bank;
    PROGRAM_ROM_BANK_LOAD bank_load;
    PROGRAM_ROM_BANK_SIZE bank_size;
};

class CharacterRomBank : public MemoryRegion {
public:
    CharacterRomBank(std::shared_ptr<System>&, int, std::string const&, CHARACTER_ROM_BANK_LOAD, CHARACTER_ROM_BANK_SIZE);
    virtual ~CharacterRomBank() {}

    bool GetGlobalMemoryLocation(u32, GlobalMemoryLocation*) override;

    bool Save(std::ostream&, std::string&) override;
    static std::shared_ptr<CharacterRomBank> Load(std::istream&, std::string&, std::shared_ptr<System>&);
private:
    int chr_rom_bank;
    CHARACTER_ROM_BANK_LOAD bank_load;
    CHARACTER_ROM_BANK_SIZE bank_size;
};

class RAMRegion : public MemoryRegion {
public:
    RAMRegion(std::shared_ptr<System>&, std::string const&, u32, u32);
    virtual ~RAMRegion() {}

    bool Load(std::istream&, std::string&);
};

class PPURegistersRegion : public MemoryRegion {
public:
    PPURegistersRegion(std::shared_ptr<System>&);
    virtual ~PPURegistersRegion() {}

    bool Load(std::istream&, std::string&);
};

class IORegistersRegion : public MemoryRegion {
public:
    IORegistersRegion(std::shared_ptr<System>&);
    virtual ~IORegistersRegion() {}

    bool Load(std::istream&, std::string&);
};

class MemoryView {
public:
    virtual u8 Read(u16) = 0;
    virtual void Write(u16, u8) = 0;

    // Peek can have different meanings depending on the memory view, but
    // the general idea is that Peeking at memory should have no side effects
    // i.e., clearing the VBL flag in the PPU
    virtual u8 Peek(u16 address) { return Read(address); }

    // NES has essentially two buses, the normal CPU bus and one private to the PPU
    // the PPU memory can be mapped to cartridges, but is normally backed by internal RAM
    virtual u8 ReadPPU(u16) = 0;
    virtual void WritePPU(u16, u8) = 0;
    virtual u8 PeekPPU(u16 address) { return ReadPPU(address); }
};

} // namespace Systems::NES

// Utility to make it easier to use c++ std types
template <>
struct std::hash<Systems::NES::GlobalMemoryLocation> : public Systems::NES::GlobalMemoryLocation::HashFunction {};

template <>
struct std::equal_to<Systems::NES::GlobalMemoryLocation> {
    bool operator()(Systems::NES::GlobalMemoryLocation const& a, Systems::NES::GlobalMemoryLocation const& b) const {
        return a == b;
    }
};

