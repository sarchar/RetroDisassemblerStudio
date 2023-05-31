#include <algorithm>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "systems/nes/cartridge.h"

using namespace std;

namespace Systems::NES {

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

    u8 chr_bank_index = 0;
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
            // MMC1 CHR banks can be swapped into high or low, so we make 4K banks and set them low always
            cout << "[NES::Cartridge::Prepare] warning: MMC1 cartridge has CHR ROM banks that aren't handled well" << endl;
            load_address = CHARACTER_ROM_BANK_LOAD_LOW;
            bank_size    = CHARACTER_ROM_BANK_SIZE_4K;
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
        ss << "CHRROM$" << hex << setfill('0') << setw(2) << uppercase << chr_bank_index;
        auto bank = make_shared<CharacterRomBank>(system, chr_bank_index++, ss.str(), load_address, bank_size);
        character_rom_banks.push_back(bank);

        if(bank_size == CHARACTER_ROM_BANK_SIZE_4K) { // make a second 4K bank
            stringstream ss;
            ss << "CHRROM$" << hex << setfill('0') << setw(2) << uppercase << chr_bank_index;
            auto bank = make_shared<CharacterRomBank>(system, chr_bank_index++, ss.str(), load_address, bank_size);
            character_rom_banks.push_back(bank);
        }
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

// relative_address is 0-0x3FFF
u8 Cartridge::ReadProgramRomRelative(int bank, u16 relative_address)
{
    auto memory_region = program_rom_banks[bank];
    return memory_region->ReadByte(relative_address + memory_region->GetBaseAddress());
}

u8 Cartridge::ReadCharacterRomRelative(int bank, u16 relative_address)
{
    auto memory_region = character_rom_banks[bank];
    return memory_region->ReadByte(relative_address + memory_region->GetBaseAddress());
}

shared_ptr<MemoryView> Cartridge::CreateMemoryView()
{
    return make_shared<CartridgeView>(shared_from_this());
}

CartridgeView::CartridgeView(std::shared_ptr<Cartridge> const& _cartridge)
    : cartridge(_cartridge)
{
    reset_vector_bank = cartridge->GetResetVectorBank();

    switch(cartridge->header.mapper) {
    case 0:
        break;

    case 1:
        mmc1.shift_register_count = 0;
        mmc1.prg_rom_bank         = 0;
        mmc1.prg_rom_bank_mode    = 3;
        mmc1.chr_rom_bank         = 0;
        mmc1.chr_rom_bank_mode    = 0;
        mmc1.mirroring = cartridge->header.mirroring;
        break;
    }
}

CartridgeView::~CartridgeView()
{
}

MIRRORING CartridgeView::GetNametableMirroring()
{
    switch(cartridge->header.mapper) {
    case 0:
        return cartridge->header.mirroring;

    case 1:
        return mmc1.mirroring;

    default:
        // unhandled
        assert(false);
        return MIRRORING_HORIZONTAL;
    }
}

u8 CartridgeView::Read(u16 address)
{
    if(address < 0x8000) {
        if(cartridge->header.has_sram) return sram[(address - 0x6000) & 0x1FFF];
        return 0;
    } 

    switch(cartridge->header.mapper) {
    case 0:
        if(!(address & 0x4000) || (cartridge->header.num_prg_rom_banks == 1)) {
            return cartridge->ReadProgramRomRelative(0, address & 0x3FFF);
        } else {
            return cartridge->ReadProgramRomRelative((address & 0x4000) >> 14, address & 0x3FFF);
        }
        break;

    case 1:
        switch(mmc1.prg_rom_bank_mode) {
        case 0: // 32KiB banks selected at $8000
        case 1:
            assert(false);
            return 0;

        case 2: // $8000 fixed, $C000 swappable
            if(address & 0x4000) {
                return cartridge->ReadProgramRomRelative(mmc1.prg_rom_bank, address & 0x3FFF);
            } else {
                return cartridge->ReadProgramRomRelative(0, address & 0x3FFF);
            }
            break;

        case 3: // $8000 swappable, $C000 fixed
            if(address & 0x4000) {
                return cartridge->ReadProgramRomRelative(reset_vector_bank, address & 0x3FFF);
            } else {
                return cartridge->ReadProgramRomRelative(mmc1.prg_rom_bank, address & 0x3FFF);
            }
            break;
        }
        break;

    default:
        // don't know how to read this mapper yet
        assert(false);
        break;
    }

    return 0;
}

void CartridgeView::Write(u16 address, u8 value)
{
    if(address < 0x8000) {
        if(cartridge->header.has_sram) sram[(address - 0x6000) & 0x1FFF] = value;
    } else {
        switch(cartridge->header.mapper) {
        case 0: // No mapper
            // ignore
            break;

        case 1: // MMC1
            // MMC1 uses a common shift register for all addresses, and writing a 1 in bit 7 clears the shift register to its initial state
            // TODO MMC1 also ignores consecutive-cycle writes, which will be a royal PITA to implement. Somehow we have to know that a 
            // read anywhere or write somewhere else occurred...
            if(value & 0x80) {
                mmc1.shift_register_count = 0;
                mmc1.prg_rom_bank_mode    = 3;
            } else {
                mmc1.shift_register = ((mmc1.shift_register >> 1) | ((value & 1) << 4)) & 0x1F;
                if(++mmc1.shift_register_count == 5) {
                    mmc1.shift_register_count = 0;

                    switch((address & 0xE000)) {
                    case 0x8000: // Control
                        switch(mmc1.shift_register & 0x03) {
                        case 0: mmc1.mirroring = MIRRORING_FOUR_SCREEN; break; //TODO
                        case 1: mmc1.mirroring = MIRRORING_FOUR_SCREEN; break; //TODO
                        case 2: mmc1.mirroring = MIRRORING_VERTICAL   ; break;
                        case 3: mmc1.mirroring = MIRRORING_HORIZONTAL ; break;
                        }

                        mmc1.prg_rom_bank_mode = (mmc1.shift_register >> 2) & 0x03;
                        mmc1.chr_rom_bank_mode = (mmc1.shift_register >> 4) & 0x01;
                        break;

                    case 0xA000: // CHR bank 0
                        mmc1.chr_rom_bank = mmc1.shift_register;
                        break;

                    case 0xC000: // CHR bank 1
                        mmc1.chr_rom_bank_high = mmc1.shift_register;
                        break;

                    case 0xE000: // PRG bank
                        mmc1.prg_rom_bank = mmc1.shift_register & 0x0F;
                        break;
                    }
                }
            }
            break;

        default:
            cout << "[CartridgeView::Write] unhandled write $" << hex << uppercase << setw(2) << setfill('0') << (int)value
                 << " to $" << setw(4) << address << endl;
            assert(false);
            break;
        }
    }
}

u8 CartridgeView::ReadPPU(u16 address)
{
    // no CHR ROM? check if we have CHR-RAM
    if(cartridge->header.num_chr_rom_banks == 0) return chr_ram[address & 0x1FFF];
    
    // check CHR-ROM banking
    switch(cartridge->header.mapper) {
    case 0:
        // One 8KiB bank at $0000-$1FFF
        return cartridge->ReadCharacterRomRelative(0, address & 0x1FFF);

    case 1:
        if(mmc1.chr_rom_bank_mode) { // switch two separate 4KiB banks
            if(address & 0x2000) { // high bank
                return cartridge->ReadCharacterRomRelative(mmc1.chr_rom_bank_high, address & 0x0FFF);
            } else { // low bank
                return cartridge->ReadCharacterRomRelative(mmc1.chr_rom_bank, address & 0x0FFF);
            }
        } else { // switch one 8KiB bank
            if(address & 0x2000) {
                return cartridge->ReadCharacterRomRelative(mmc1.chr_rom_bank + 1, address & 0x0FFF);
            } else {
                return cartridge->ReadCharacterRomRelative(mmc1.chr_rom_bank, address & 0x0FFF);
            }
        }
        break;

    default:
        assert(false);
        break;
    }

    return 0;
}

void CartridgeView::WritePPU(u16 address, u8 value)
{
    if(cartridge->header.num_chr_rom_banks == 0) chr_ram[address & 0x1FFF] = value;
}

}
