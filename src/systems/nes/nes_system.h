#pragma once

#include <iostream>
#include <iomanip>
#include <sstream>

#include "systems/system.h"

class NESSystem; // TODO move into NES namespace

namespace NES {

class Cartridge;
class ProgramRomBank;

enum MIRRORING {
    MIRRORING_HORIZONTAL = 0,
    MIRRORING_VERTICAL,
    MIRRORING_FOUR_SCREEN,
};

enum CONTENT_BLOCK_TYPE {
    CONTENT_BLOCK_TYPE_DATA = 0,
    CONTENT_BLOCK_TYPE_CODE,
    CONTENT_BLOCK_TYPE_CHR,
};

enum CONTENT_BLOCK_DATA_TYPE {
    CONTENT_BLOCK_DATA_TYPE_UBYTE = 0,
    CONTENT_BLOCK_DATA_TYPE_UWORD
};

// Content blocks can be data, code, or graphics
// Data can be structured, arrays, etc. So we will need types, arrays, etc
// Will need built-in types like byte, word, function pointer, etc.
struct ContentBlock {
    CONTENT_BLOCK_TYPE type;    // CODE statement, DATA block, CHR, possibly other things
    u16                offset;  // Offset within the rom bank that this content block starts at

    union {
        struct {
            // code contentblock is always one instruction long
        } code;

        struct {
            CONTENT_BLOCK_DATA_TYPE type; // byte, word, pointer, user defined
            u16   count;   // Total number of elements
            void* ptr;     // TODO will need a structure to hold various content blocks
        } data;

        struct {
        } chr;
    };

    u32 GetDataTypeSize() const {
        switch(data.type) {
        case CONTENT_BLOCK_DATA_TYPE_UBYTE:
            return 1;
        case CONTENT_BLOCK_DATA_TYPE_UWORD:
            return 2;
        default:
            assert(false);
            return 0;
        }
    }

    // return size in bytes
    u32 GetSize() const {
        switch(type) {
        case CONTENT_BLOCK_TYPE_DATA:
            return data.count * GetDataTypeSize();

        default:
            assert(false); // TODO
            return 0;
        }
    }

    std::string FormatInstructionField() {
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

    std::string FormatDataElement(u16 n) {
        std::stringstream ss;

        assert(type == CONTENT_BLOCK_TYPE_DATA);
        switch(data.type) {
        case CONTENT_BLOCK_DATA_TYPE_UBYTE:
            ss << "$" << std::hex << std::setfill('0') << std::setw(2) << (u16)((u8*)data.ptr)[n];
            break;

        case CONTENT_BLOCK_DATA_TYPE_UWORD:
        {
            u16* p = &((u16*)data.ptr)[n];
            u16  v = p[0] | (p[1] << 8);
            ss << "$" << std::hex << std::setfill('0') << std::setw(4) << v;
            break;
        }

        default:
            break;
        }

        return ss.str();
    }
};

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

// A single ListingItem translates to a single row in the Listing window. A listing item can be
// all sorts of row types: comments, labels, actual code, data, etc.
class ListingItem {
public:
    enum TYPE {
        LISTING_ITEM_TYPE_UNKNOWN,
        LISTING_ITEM_TYPE_DATA
    };

    ListingItem(TYPE type, GlobalMemoryLocation const& where) :
        item_type(type), global_memory_location(where) 
    { 
    }
    virtual ~ListingItem() { }

    //TYPE GetType() const { return item_type; }
    virtual void RenderContent(std::shared_ptr<NESSystem>&) = 0;

protected:
    TYPE                 item_type;
    GlobalMemoryLocation global_memory_location;
};

class ListingItemUnknown : public ListingItem {
public:
    ListingItemUnknown(GlobalMemoryLocation const& where)
        : ListingItem(ListingItem::LISTING_ITEM_TYPE_UNKNOWN, where)      
    { 
    }
    virtual ~ListingItemUnknown() { }

    void RenderContent(std::shared_ptr<NESSystem>&) override;
};

class ListingItemData : public ListingItem {
public:
    ListingItemData(GlobalMemoryLocation const& where, std::shared_ptr<ProgramRomBank>& _prg_bank)
        : ListingItem(ListingItem::LISTING_ITEM_TYPE_DATA, where), prg_bank(_prg_bank) 
    { 
    }
    virtual ~ListingItemData() { }

    void RenderContent(std::shared_ptr<NESSystem>&) override;

private:
    std::shared_ptr<ProgramRomBank> prg_bank;
};


}

class NESSystem : public System {
public:
    NESSystem();
    virtual ~NESSystem();

    System::Information const* GetInformation();
    bool CreateNewProjectFromFile(std::string const&);

    // creation interface
    static System::Information const* GetInformationStatic();
    static bool IsROMValid(std::string const& file_path_name, std::istream& is);
    static std::shared_ptr<System> CreateSystem();

    std::shared_ptr<NES::Cartridge>& GetCartridge() { return cartridge; }
    std::shared_ptr<NES::Cartridge> cartridge;

    // Memory
    void GetEntryPoint(NES::GlobalMemoryLocation*);
    u16  GetSegmentBase(NES::GlobalMemoryLocation const&);
    u32  GetSegmentSize(NES::GlobalMemoryLocation const&);

    // Content
    std::shared_ptr<NES::ContentBlock>& GetContentBlockAt(NES::GlobalMemoryLocation const&);
    void MarkContentAsData(NES::GlobalMemoryLocation const&, u32 byte_count, NES::CONTENT_BLOCK_DATA_TYPE data_type);

    // Listings
    void GetListingItems(NES::GlobalMemoryLocation const&, std::vector<std::shared_ptr<NES::ListingItem>>& out);

private:
    std::string rom_file_path_name;
};


