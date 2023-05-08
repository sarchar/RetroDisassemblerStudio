#pragma once

#include <cassert>
#include <iostream>
#include <iomanip>
#include <vector>

#include "util.h"

#include "systems/nes/nes_defs.h"

namespace NES {

class ContentBlock;

// SystemMemoryLocation dials into a specific byte within the system. It has enough information to select which
// segment of the system (RAM, SRAM, etc) as well as which ROM bank, overlay or any psuedo location that may exist.
struct GlobalMemoryLocation {
    GlobalMemoryLocation() { 
        address      = 0;
        is_chr       = false;
        prg_rom_bank = 0;
        chr_rom_bank = 0;
    }

    // 0x00-0xFF: zero page
    // 0x100-0x1FF: stack
    // 0x200-0x7FF: RAM
    // 0x6000-7FFF: SRAM
    // 0x8000-FFFF: ROM
    u16 address;

    // set to true if we're reading CHR-RAM
    bool is_chr;

    // used only for ROM
    u16 prg_rom_bank;

    // used only for CHR
    u16 chr_rom_bank;

    void Increment() {
    }

    template<typename T>
    GlobalMemoryLocation operator+(T const& v) const {
        GlobalMemoryLocation ret(*this);
        ret.address += v; // TODO wrap, increment banks, etc
        return ret;
    }

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
};

// MemoryRegion represents a region of memory on the system
// Memory regions are a list of content ordered by the contents offset in the block
// But because lookups would be slow with blocks of content, we still have a pointer into the content table for each address in the region
class MemoryRegion {
public:
    typedef std::vector<std::shared_ptr<ContentBlock>> ContentBlockListType;

    MemoryRegion() 
        : content_ptrs(NULL) { }
    ~MemoryRegion();

    u32 GetBaseAddress()       const { return base_address; }
    u16 GetRegionSize()        const { return region_size; }
    u32 GetTotalListingItems() const { return total_listing_items; }

    inline u32 ConvertToOffset(u32 address_in_region) { 
        assert(address_in_region >= base_address && address_in_region < base_address + region_size);
        return address_in_region - base_address; 
    }

    void                           InitializeWithData(u16 offset, u16 count, u8* data);

    ContentBlockListType::iterator InsertContentBlock(ContentBlockListType::iterator, std::shared_ptr<ContentBlock>& content_block);

    std::shared_ptr<ContentBlock>  GetContentBlockAt(GlobalMemoryLocation const& where);

    std::shared_ptr<ContentBlock>  SplitContentBlock(GlobalMemoryLocation const& where);

    void                           MarkContentAsData(GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type);

    // Listing help
    u32  GetListingIndexByAddress(GlobalMemoryLocation const&);
    u32  GetAddressForListingItemIndex(u32 listing_item_index);

    virtual u8  ReadByte(GlobalMemoryLocation const&) = 0;

    void PrintContentBlocks();

protected:
    u32 base_address;
    u32 region_size;
    u32 total_listing_items;

private:

    void Erase();
    ContentBlockListType content;

    // an array mapping an address into its respective content block
    //u16* content_ptrs;
    std::shared_ptr<std::weak_ptr<ContentBlock>[]> content_ptrs;

    // during emulation, we will want a cache for already translated code
    // u8 opcode_cache[];
    // and probably a bitmap indicating whether an address is valid in the cache
    // u64 opcode_cache_valid[16 * 1024 / 64] // one bit per address using 64 bit ints
};

class ProgramRomBank : public MemoryRegion {
public:
    ProgramRomBank(PROGRAM_ROM_BANK_LOAD, PROGRAM_ROM_BANK_SIZE);
    virtual ~ProgramRomBank() {};

    u8 ReadByte(GlobalMemoryLocation const&) override { return 0; }
private:
    PROGRAM_ROM_BANK_LOAD bank_load;
    PROGRAM_ROM_BANK_SIZE bank_size;
};

class CharacterRomBank : public MemoryRegion {
public:
    CharacterRomBank(CHARACTER_ROM_BANK_LOAD, CHARACTER_ROM_BANK_SIZE);
    virtual ~CharacterRomBank() {}

    u8 ReadByte(GlobalMemoryLocation const&) override { return 0; }

private:
    CHARACTER_ROM_BANK_LOAD bank_load;
    CHARACTER_ROM_BANK_SIZE bank_size;
};


}
