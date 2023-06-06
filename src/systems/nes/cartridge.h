// Copyright (c) 2023, Charles Mason <chuck+github@borboggle.com>
// All rights reserved.
// 
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. 
#pragma once

#include <memory>

#include "systems/nes/memory.h"
#include "systems/nes/system.h"

namespace Systems::NES {

class CartridgeView;
class System;

class Cartridge : public std::enable_shared_from_this<Cartridge> {
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
    std::shared_ptr<RAMRegion> const&  GetSRAM() { return sram; }
    std::shared_ptr<ProgramRomBank>&   GetProgramRomBank(u8 bank) { return program_rom_banks[bank]; }
    std::shared_ptr<CharacterRomBank>& GetCharacterRomBank(u8 bank) { return character_rom_banks[bank]; }
    int                                GetNumMemoryRegions() const;
    int                                GetNumCharacterRomBanks() const { return character_rom_banks.size(); }
    std::shared_ptr<MemoryRegion>      GetMemoryRegion(GlobalMemoryLocation const&);
    std::shared_ptr<MemoryRegion>      GetMemoryRegionByIndex(int);

    std::shared_ptr<MemoryView>        CreateMemoryView();

    u16 GetResetVectorBank();

    void NoteReferences();

    u8 ReadProgramRomRelative(int, u16);
    u8 ReadCharacterRomRelative(int, u16);
    void CopyCharacterRomRelative(int, u8*, u16, u16);

    bool Save(std::ostream&, std::string&);
    bool Load(std::istream&, std::string&, std::shared_ptr<System>&);
private:
    void CreateMemoryRegions();

    std::weak_ptr<System> parent_system;

    std::shared_ptr<RAMRegion>                     sram;
    std::vector<std::shared_ptr<ProgramRomBank>>   program_rom_banks;
    std::vector<std::shared_ptr<CharacterRomBank>> character_rom_banks;
};

class CartridgeView : public MemoryView {
public:
    CartridgeView(std::shared_ptr<Cartridge> const&);
    ~CartridgeView();

    MIRRORING GetNametableMirroring();
    int       GetRomBank(u16);

    // Peek can be mapped to Read()
    // since CartridgeView doesn't have side-effects currently
    u8 Read(u16) override;
    void Write(u16, u8) override;

    u8 ReadPPU(u16) override;
    void WritePPU(u16, u8) override;

    void CopyPatterns(u8*, u16, u16);

    friend class Cartridge;

private:
    int SelectCHRRomBankForAddress(u16&);
    std::shared_ptr<Cartridge> cartridge;

    u8 reset_vector_bank;

    union {
        struct {
            u8 shift_register;
            u8 shift_register_count;
            u8 chr_rom_bank;
            u8 chr_rom_bank_high;
            u8 prg_rom_bank;
            u8 prg_rom_bank_mode;
            u8 chr_rom_bank_mode;
            MIRRORING mirroring;
        } mmc1;

        struct {
            u8 prg_rom_bank;
        } mmc2;
    };

    u8  sram[0x2000];
    u8  chr_ram[0x2000];
};

} // namespace Systems::NES

