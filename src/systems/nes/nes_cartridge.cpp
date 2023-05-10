#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>

#include "systems/nes/nes_cartridge.h"

using namespace std;

namespace NES {

Cartridge::Cartridge(shared_ptr<System>& system) 
{
    parent_system = system;
}

Cartridge::~Cartridge() 
{
}

void Cartridge::Prepare()
{
    auto system = parent_system.lock();
    if(!system) return;

    assert(program_rom_banks.size() == 0);

    for(u32 i = 0; i < header.num_prg_rom_banks; i++) {
        PROGRAM_ROM_BANK_LOAD load_address;
        PROGRAM_ROM_BANK_SIZE bank_size;

        bank_size = PROGRAM_ROM_BANK_SIZE_16K;

        switch(header.mapper) {
        case 0: { // No memory mapper, we can either have 1 16KiB block loaded at 0xC000, or two 16KiB blocks filling the entire 32KiB region
            if(header.num_prg_rom_banks == 1) {
                load_address = PROGRAM_ROM_BANK_LOAD_HIGH_16K;
            } else {
                assert(header.num_prg_rom_banks == 2);
                load_address = (i == 0 ? PROGRAM_ROM_BANK_LOAD_LOW_16K : PROGRAM_ROM_BANK_LOAD_HIGH_16K);
            }
            break;
        }

        case 1: { // MMC1
            assert(header.num_prg_rom_banks >= 2);
            load_address = ((i == 15 || i == (header.num_prg_rom_banks - 1)) ? PROGRAM_ROM_BANK_LOAD_HIGH_16K : PROGRAM_ROM_BANK_LOAD_LOW_16K);
            break;
        }
        
        default:
            assert(false); // Unhandled mapper
        }

        auto bank = make_shared<ProgramRomBank>(system, load_address, bank_size);
        program_rom_banks.push_back(bank);
    }

    for(u32 i = 0; i < header.num_chr_rom_banks; i++) {
        CHARACTER_ROM_BANK_LOAD load_address;
        CHARACTER_ROM_BANK_SIZE bank_size;

        switch(header.mapper) {
        case 0: { // No memory mapper, we can either have 1 8KiB block or not
            assert(header.num_chr_rom_banks <= 1);
            load_address = CHARACTER_ROM_BANK_LOAD_LOW;
            bank_size    = CHARACTER_ROM_BANK_SIZE_8K;
            break;
        }

        case 1: { // MMC1
            // TODO MMC1 also supports 4K CHR banks that can be loaded into *either* low or high
            // will have to support memory regions that can change their base? or at least, leave 
            // it unset until the user specifies the base_address. but for now, we assume they're 
            // swapped at 8K and always in the full memory
            cout << "[NES::Cartridge::Prepare] warning: MMC1 cartridge has CHR ROM banks that aren't handled well" << endl;
            load_address = CHARACTER_ROM_BANK_LOAD_LOW;
            bank_size    = CHARACTER_ROM_BANK_SIZE_8K;
            break;
        }
        
        default:
            assert(false); // Unhandled mapper
        }

        auto bank = make_shared<CharacterRomBank>(system, load_address, bank_size);
        character_rom_banks.push_back(bank);
    }
}

u16 Cartridge::GetResetVectorBank()
{
    switch(header.mapper) {
    case 0:
        return header.num_prg_rom_banks == 2 ? 1 : 0;

    case 1:
        // lower 256KiB starts selected, so limit to the 16th bank
        return min((u16)16, (u16)header.num_prg_rom_banks) - 1;
    }

    return 0;
}

//!void Cartridge::MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type)
//!{
//!    assert(where.address >= 0x8000); // TODO support SRAM etc
//!
//!    auto prg_bank = GetProgramRomBank(where.prg_rom_bank);
//!    prg_bank->MarkContentAsData(where, byte_count, new_data_type);
//!}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//!u8 ProgramRomBank::ReadByte(u16 offset)
//!{
//!    assert(false);
//!    return 0xEA;
//!    u16 cref = content_ptrs[offset];
//!    if(cref == (u16)-1) return 0xEA;
//!
//!    auto& c = content[cref];
//!    assert(c->type == CONTENT_BLOCK_TYPE_DATA);
//!    assert((offset - c->offset) < c->data.count); // TODO take into account data size
//!    return static_cast<u8*>(c->data.ptr)[offset - c->offset];
//!}

}
