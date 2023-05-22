#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

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

bool Cartridge::LoadHeader(u8* buf)
{
    // Parse the iNES header
    header.num_prg_rom_banks = (u8)buf[4];
    header.num_chr_rom_banks = (u8)buf[5];
    header.prg_rom_size      = header.num_prg_rom_banks * 16 * 1024;
    header.chr_rom_size      = header.num_chr_rom_banks *  8 * 1024;
    header.mapper            = ((u8)buf[6] & 0xF0) >> 4 | ((u8)buf[7] & 0xF0);
    header.mirroring         = ((u8)buf[6] & 0x08) ? MIRRORING_FOUR_SCREEN : ((bool)((u8)buf[6] & 0x01) ? MIRRORING_VERTICAL : MIRRORING_HORIZONTAL);
    header.has_sram          = (bool)((u8)buf[6] & 0x02);
    header.has_trainer       = (bool)((u8)buf[6] & 0x04);

    // Finish creating the cartridge based on mapper information
    CreateMemoryRegions();

    return true;
}

void Cartridge::CreateMemoryRegions()
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
        
        case 2: { // MMC2
            load_address = (i == (header.num_prg_rom_banks - 1)) ? PROGRAM_ROM_BANK_LOAD_HIGH_16K : PROGRAM_ROM_BANK_LOAD_LOW_16K;
            break;
        }

        default:
            assert(false); // Unhandled mapper
        }

        stringstream ss;
        ss << "PRGROM$" << hex << setfill('0') << setw(2) << uppercase << i;
        auto bank = make_shared<ProgramRomBank>(system, i, ss.str(), load_address, bank_size);
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
            // swapped at 8K and always in the full memory. In fact it's probably not important
            // what their base is, since they don't have address references contained within in the bank
            // we could assume they all start at 0
            cout << "[NES::Cartridge::Prepare] warning: MMC1 cartridge has CHR ROM banks that aren't handled well" << endl;
            load_address = CHARACTER_ROM_BANK_LOAD_LOW;
            bank_size    = CHARACTER_ROM_BANK_SIZE_8K;
            break;
        }

        case 2: { // MMC2 does not have bankable CHR-ROM
            load_address = CHARACTER_ROM_BANK_LOAD_LOW;
            bank_size    = CHARACTER_ROM_BANK_SIZE_8K;
            break;
        }
        
        default:
            assert(false); // Unhandled mapper
        }

        stringstream ss;
        ss << "CHRROM$" << hex << setfill('0') << setw(2) << uppercase << i;
        auto bank = make_shared<CharacterRomBank>(system, i, ss.str(), load_address, bank_size);
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

    case 2:
        return (u16)header.num_prg_rom_banks - 1;
    }

    return 0;
}

bool Cartridge::CanBank(GlobalMemoryLocation const& where)
{
    if(where.address < 0x8000) {
        return false;
    } else {
        switch(header.mapper) {
        case 0: // no banking with mapper 0
            return false;

        case 1: // MMC1 depends on the location 
            if(header.num_prg_rom_banks <= 16 && where.address >= 0xC000) return false;
            return true;

        case 2: // Only 0x8000-0xBFFF is bankable
            return where.address < 0xC000;

        default:
            assert(false);
            return false;
        }
    }

}

int Cartridge::GetNumMemoryRegions() const
{
    // TODO need one for SRAM?
    return program_rom_banks.size() + character_rom_banks.size();
}

std::shared_ptr<MemoryRegion> Cartridge::GetMemoryRegionByIndex(int i)
{
    if(i >= program_rom_banks.size()) return character_rom_banks.at(i - program_rom_banks.size());
    return program_rom_banks.at(i);
}

std::shared_ptr<MemoryRegion> Cartridge::GetMemoryRegion(GlobalMemoryLocation const& where)
{
    if(where.is_chr) {
        return nullptr;
    } else {
        if(where.address < 0x8000) { // TODO SRAM support
            return nullptr;
        } else {
            switch(header.mapper) {
            case 0: // easy
                if(header.num_prg_rom_banks == 1) return program_rom_banks[0];
                return program_rom_banks[(int)(where.address >= 0xC000)];

            case 1: 
                if(header.num_prg_rom_banks <= 16 && where.address >= 0xC000) {
                    return program_rom_banks[header.num_prg_rom_banks - 1];
                } else {
                    assert(where.prg_rom_bank < header.num_prg_rom_banks);
                    return program_rom_banks[where.prg_rom_bank];
                }

            case 2:
                if(where.address < 0xC000) return program_rom_banks[where.prg_rom_bank];
                else return program_rom_banks[header.num_prg_rom_banks - 1];

            default:
                assert(false);
                return nullptr;
            }
        }
    }
}

bool Cartridge::Save(std::ostream& os, std::string& errmsg)
{
    os.write((char*)&header, sizeof(header));
    if(!os.good()) {
        errmsg = "Error writing cartridge header";
        return false;
    }

    for(auto& memory_region : program_rom_banks) if(!memory_region->Save(os, errmsg)) return false;
    for(auto& memory_region : character_rom_banks) if(!memory_region->Save(os, errmsg)) return false;

    return true;
}

bool Cartridge::Load(std::istream& is, std::string& errmsg, shared_ptr<System>& system)
{
    is.read((char*)&header, sizeof(header));
    if(!is.good()) {
        errmsg = "Error reading cartridge header";
        return false;
    }

    for(u32 i = 0; i < header.num_prg_rom_banks; i++) {
        auto memory_region = ProgramRomBank::Load(is, errmsg, system);
        if(!memory_region) return false;
        program_rom_banks.push_back(memory_region);
    }

    for(u32 i = 0; i < header.num_chr_rom_banks; i++) {
        auto memory_region = CharacterRomBank::Load(is, errmsg, system);
        if(!memory_region) return false;
        character_rom_banks.push_back(memory_region);
    }

    return true;
}

void Cartridge::NoteReferences()
{
    for(auto& prg_rom : program_rom_banks) {
        prg_rom->NoteReferences();
    }
}

}
