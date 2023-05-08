#pragma once

// Need NES defines
#include "systems/nes/nes_system.h"

namespace NES {

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

// ROM banks are a list of content ordered by the contents offset in the bank
// But because lookups would be slow with blocks of content, we still have a pointer into the content table
// for each address in the bank
class ProgramRomBank {
public:
    ProgramRomBank(PROGRAM_ROM_BANK_LOAD, PROGRAM_ROM_BANK_SIZE);
    ~ProgramRomBank();

    void InitializeAsBytes(u16 offset, u16 count, u8* data);

    u16 GetActualLoadAddress() {
        return (load_address == PROGRAM_ROM_BANK_LOAD_LOW_16K) ? 0x8000 : 0xC000;
    }

    u16 GetActualSize() {
        return (bank_size == PROGRAM_ROM_BANK_SIZE_32K) ? 0x8000 : 0x4000;
    }

    std::shared_ptr<ContentBlock> GetContentBlockAt(GlobalMemoryLocation const& where);
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

    Cartridge();
    ~Cartridge();
    void Prepare(void);

    std::shared_ptr<ProgramRomBank>&   GetProgramRomBank(u8 bank) { return program_rom_banks[bank]; }
    std::shared_ptr<CharacterRomBank>& GetCharacterRomBank(u8 bank) { return character_rom_banks[bank]; }

    u16 GetResetVectorBank();
private:
    std::vector<std::shared_ptr<ProgramRomBank>>   program_rom_banks;
    std::vector<std::shared_ptr<CharacterRomBank>> character_rom_banks;
};

}

