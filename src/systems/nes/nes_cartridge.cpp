#include <cassert>
#include <iomanip>
#include <iostream>
#include <memory>

#include "systems/nes/nes_cartridge.h"

using namespace std;

namespace NES {

Cartridge::Cartridge() 
{
}

Cartridge::~Cartridge() 
{
}

void Cartridge::Prepare()
{
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
            assert(false);
            break;
        }
        
        default:
            assert(false); // Unhandled mapper
        }

        auto bank = make_shared<ProgramRomBank>(load_address, bank_size);
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
            assert(false);
            break;
        }
        
        default:
            assert(false); // Unhandled mapper
        }

        auto bank = make_shared<CharacterRomBank>(load_address, bank_size);
        character_rom_banks.push_back(bank);
    }
}

u16 Cartridge::GetResetVectorBank()
{
    switch(header.mapper) {
    case 0:
        return header.num_prg_rom_banks == 2 ? 1 : 0;

    case 1:
        assert(false); // TODO
    }

    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
ProgramRomBank::ProgramRomBank(PROGRAM_ROM_BANK_LOAD _load_address, PROGRAM_ROM_BANK_SIZE _bank_size)
    : load_address(_load_address),
      bank_size(_bank_size),
      content_ptrs(NULL)
{
}

ProgramRomBank::~ProgramRomBank()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
    }

    for(auto& blk : content) {
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.data_ptr != NULL) {
            delete [] blk->data.data_ptr;
        }
    }
}

void ProgramRomBank::Erase()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
        content_ptrs = NULL;
    }

    content.clear();

    switch(bank_size) {
    case PROGRAM_ROM_BANK_SIZE_16K:
        content_ptrs = new u16[16 * 1024];
        memset(content_ptrs, (u16)-1, 16 * 1024);
        break;

    default:
        assert(false); // TODO
        break;
    }
}

void ProgramRomBank::InitializeAsBytes(u16 offset, u16 count, u8* data)
{
    assert(offset + count <= 16 * 1024); // make sure we don't overflow the bank

    // Kill all content blocks and references
    Erase();

    // Set the only content block to be one big array of bytes
    shared_ptr<ContentBlock> blk = make_shared<ContentBlock>();
    blk->type = CONTENT_BLOCK_TYPE_DATA;
    blk->offset = offset;
    blk->data.type = CONTENT_BLOCK_DATA_TYPE_UBYTE;
    blk->data.count = count;

    u32 size = sizeof(u8) * count;
    blk->data.data_ptr = new u8[size];
    memcpy(blk->data.data_ptr, data, size);

    // Add the first content
    u16 ref = content.size();
    assert(ref == 0);
    content.push_back(blk);
    
    // Set all the address pointers
    for(u32 s = offset; s < (u32)offset + (u32)count; s++) {
        content_ptrs[s] = ref;
    }

    cout << "[ProgramRomBank::InitializeAsBytes] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data starting at bank offset 0x" << setw(4) << offset
         << " base 0x" << (load_address == PROGRAM_ROM_BANK_LOAD_LOW_16K ? 0x8000 : 0xC000) << endl;
}

std::shared_ptr<ContentBlock> ProgramRomBank::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    u16 base = where.address;
    if(load_address == PROGRAM_ROM_BANK_LOAD_LOW_16K) {
        base -= 0x8000;
    } else if(load_address == PROGRAM_ROM_BANK_LOAD_HIGH_16K) {
        base -= 0xC000;
    }

    u16 cref = content_ptrs[base];
    if(cref == (u16)-1) return nullptr;

    return content[cref];
}

u8 ProgramRomBank::ReadByte(u16 offset)
{
    u16 cref = content_ptrs[offset];
    if(cref == (u16)-1) return 0xEA;

    auto& c = content[cref];
    assert(c->type == CONTENT_BLOCK_TYPE_DATA);
    assert((offset - c->offset) < c->data.count); // TODO take into account data size
    return static_cast<u8*>(c->data.data_ptr)[offset - c->offset];
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CharacterRomBank::CharacterRomBank(CHARACTER_ROM_BANK_LOAD _load_address, CHARACTER_ROM_BANK_SIZE _bank_size)
    : load_address(_load_address),
      bank_size(_bank_size),
      content_ptrs(NULL)
{
}

CharacterRomBank::~CharacterRomBank()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
    }

    for(auto& blk : content) {
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.data_ptr != NULL) {
            delete [] blk->data.data_ptr;
        }
    }
}

void CharacterRomBank::Erase()
{
    if(content_ptrs != NULL) {
        delete [] content_ptrs;
        content_ptrs = NULL;
    }

    content.clear();

    switch(bank_size) {
    case CHARACTER_ROM_BANK_SIZE_8K:
        content_ptrs = new u16[8 * 1024];
        memset(content_ptrs, (u16)-1, 8 * 1024);
        break;

    default:
        assert(false); // TODO
        break;
    }
}

void CharacterRomBank::InitializeAsBytes(u16 offset, u16 count, u8* data)
{
    assert(offset + count <= 8 * 1024); // make sure we don't overflow the bank TODO fix

    // Kill all content blocks and references
    Erase();

    // Set the only content block to be one big array of bytes
    shared_ptr<ContentBlock> blk = make_shared<ContentBlock>();
    blk->type = CONTENT_BLOCK_TYPE_DATA;
    blk->offset = offset;
    blk->data.type = CONTENT_BLOCK_DATA_TYPE_UBYTE;
    blk->data.count = count;

    u32 size = sizeof(u8) * count;
    blk->data.data_ptr = new u8[size];
    memcpy(blk->data.data_ptr, data, size);

    // Add the first content
    u16 ref = content.size();
    assert(ref == 0);
    content.push_back(blk);
    
    // Set all the address pointers
    for(u32 s = offset; s < (u32)offset + (u32)count; s++) {
        content_ptrs[s] = ref;
    }

    cout << "[CharacterRomBank::InitializeAsBytes] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data starting at bank offset 0x" << setw(4) << offset
         << " base 0x" << setw(4) << (load_address == CHARACTER_ROM_BANK_LOAD_LOW ? 0x0000 : 0x1000) << endl;
}

}
