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

    bool LoadHeader(u8*);

    bool                               CanBank(GlobalMemoryLocation const&);
    std::shared_ptr<ProgramRomBank>&   GetProgramRomBank(u8 bank) { return program_rom_banks[bank]; }
    std::shared_ptr<CharacterRomBank>& GetCharacterRomBank(u8 bank) { return character_rom_banks[bank]; }
    std::shared_ptr<MemoryRegion>      GetMemoryRegion(GlobalMemoryLocation const&);

    u16 GetResetVectorBank();

    bool Save(std::ostream&, std::string&);
    bool Load(std::istream&, std::string&, std::shared_ptr<System>&);
private:
    void CreateMemoryRegions();

    std::weak_ptr<System> parent_system;

    std::vector<std::shared_ptr<ProgramRomBank>>   program_rom_banks;
    std::vector<std::shared_ptr<CharacterRomBank>> character_rom_banks;
};

}

