#pragma once

#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

namespace NES {

class System;

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

    Cartridge(std::shared_ptr<System>&);
    ~Cartridge();
    void Prepare(void);

    std::shared_ptr<ProgramRomBank>&   GetProgramRomBank(u8 bank) { return program_rom_banks[bank]; }
    std::shared_ptr<CharacterRomBank>& GetCharacterRomBank(u8 bank) { return character_rom_banks[bank]; }

    u16 GetResetVectorBank();

//!    void MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type);
private:
    std::weak_ptr<System> parent_system;

    std::vector<std::shared_ptr<ProgramRomBank>>   program_rom_banks;
    std::vector<std::shared_ptr<CharacterRomBank>> character_rom_banks;
};

}

