#pragma once

// Need NES defines
#include "systems/nes/nes_system.h"

namespace NES {

enum CONTENT_BLOCK_TYPE {
    CONTENT_BLOCK_TYPE_DATA = 0,
    CONTENT_BLOCK_TYPE_CODE,
    CONTENT_BLOCK_TYPE_CHR,
};

enum CONTENT_BLOCK_DATA_TYPE {
    CONTENT_BLOCK_DATA_TYPE_UBYTE = 0,
};

enum PROGRAM_ROM_BANK_LOAD {
    PROGRAM_ROM_BANK_LOAD_LOW_16K,
    PROGRAM_ROM_BANK_LOAD_HIGH_16K
};

enum PROGRAM_ROM_BANK_SIZE {
    PROGRAM_ROM_BANK_SIZE_16K,
    PROGRAM_ROM_BANK_SIZE_32K
};

enum CHARACTER_ROM_BANK_LOAD {
    CHARACTER_ROM_BANK_LOAD_LOW,
    CHARACTER_ROM_BANK_LOAD_HIGH
};

enum CHARACTER_ROM_BANK_SIZE {
    CHARACTER_ROM_BANK_SIZE_4K,
    CHARACTER_ROM_BANK_SIZE_8K
};

class Cartridge {
public:
    struct {
        u8 num_prg_rom_banks;
        u32 prg_rom_size;

        u8 num_chr_rom_banks;
        u32 chr_rom_size;

        u8 mapper;
        MIRRORING mirroring;

        bool has_sram;
        bool has_trainer;
    } header;

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
                CONTENT_BLOCK_DATA_TYPE data_type; // byte, word, pointer, user defined
                u16   count;       // Total number of elements
                void* data_ptr;    // TODO will need a structure to hold various content blocks
            } data;

            struct {
            } chr;
        };
    };

    // ROM banks are a list of content ordered by the contents offset in the bank
    // But because lookups would be slow with blocks of content, we still have a pointer into the content table
    // for each address in the bank
    class ProgramRomBank {
    public:
        ProgramRomBank(PROGRAM_ROM_BANK_LOAD, PROGRAM_ROM_BANK_SIZE);
        ~ProgramRomBank();

        void InitializeAsBytes(u16 offset, u16 count, u8* data);
        u8   ReadByte(u16 offset);
    private:
        void Erase();

        PROGRAM_ROM_BANK_LOAD load_address;
        PROGRAM_ROM_BANK_SIZE bank_size;

        std::vector<std::shared_ptr<ContentBlock>> content;

        // and an array mapping an address into its respective content block
        u16* content_ptrs;

        // during emulation, we will want a cache for already translated code
        // u8 opcode_cache[];
        // and probably a bitmap indicating whether an address is valid in the cache
        // u64 opcode_cache_valid[16 * 1024 / 64] // one bit per address using 64 bit ints
    };

    class CharacterRomBank {
    public:
        CharacterRomBank(CHARACTER_ROM_BANK_LOAD, CHARACTER_ROM_BANK_SIZE);
        ~CharacterRomBank();

        void InitializeAsBytes(u16 offset, u16 count, u8* data);
    private:
        void Erase();

        CHARACTER_ROM_BANK_LOAD load_address;
        CHARACTER_ROM_BANK_SIZE bank_size;

        std::vector<std::shared_ptr<ContentBlock>> content;

        // and an array mapping an address into its respective content block
        u16* content_ptrs;

        // during emulation, we will want a cache for already determined data
        // u8 cache[];
        // and probably a bitmap indicating whether an address is valid in the cache
        // u64 cache_valid[1 * 1024 / 64] // one bit per address using 64 bit ints
    };


    Cartridge();
    ~Cartridge();
    void Prepare(void);

    std::shared_ptr<ProgramRomBank>&   GetProgramRomBank(u8 bank) { return program_rom_banks[bank]; }
    std::shared_ptr<CharacterRomBank>& GetCharacterRomBank(u8 bank) { return character_rom_banks[bank]; }

private:
    std::vector<std::shared_ptr<ProgramRomBank>>   program_rom_banks;
    std::vector<std::shared_ptr<CharacterRomBank>> character_rom_banks;
};

}

