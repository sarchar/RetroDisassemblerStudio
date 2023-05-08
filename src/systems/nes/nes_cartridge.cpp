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

std::shared_ptr<ContentBlock>& Cartridge::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    return GetProgramRomBank(where.prg_rom_bank)->GetContentBlockAt(where);
}

void Cartridge::MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type)
{
    assert(where.address >= 0x8000); // TODO support SRAM etc

    auto prg_bank = GetProgramRomBank(where.prg_rom_bank);
    prg_bank->MarkContentAsData(where, byte_count, new_data_type);
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
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.ptr != NULL) {
            delete [] blk->data.ptr;
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
    blk->data.ptr = new u8[size];
    memcpy(blk->data.ptr, data, size);

    // Add the content to the bank
    AddContentBlock(blk);

    cout << "[ProgramRomBank::InitializeAsBytes] set 0x" << hex << uppercase << setfill('0') << setw(0) << count 
         << " bytes of data starting at bank offset 0x" << setw(4) << offset
         << " base 0x" << (load_address == PROGRAM_ROM_BANK_LOAD_LOW_16K ? 0x8000 : 0xC000) << endl;
}

void ProgramRomBank::AddContentBlock(shared_ptr<ContentBlock>& content_block)
{
    u16 ref = content.size();
    content.push_back(content_block);

    u32 size = content_block->GetSize();
    for(u32 s = content_block->offset; s < content_block->offset + size; s++) {
        content_ptrs[s] = ref;
    }
}

std::shared_ptr<ContentBlock>& ProgramRomBank::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    u16 base = ConvertToOffset(where.address);
    u16 cref = content_ptrs[base];
    return content[cref];
}

u8 ProgramRomBank::ReadByte(u16 offset)
{
    u16 cref = content_ptrs[offset];
    if(cref == (u16)-1) return 0xEA;

    auto& c = content[cref];
    assert(c->type == CONTENT_BLOCK_TYPE_DATA);
    assert((offset - c->offset) < c->data.count); // TODO take into account data size
    return static_cast<u8*>(c->data.ptr)[offset - c->offset];
}

shared_ptr<ContentBlock> ProgramRomBank::SplitContentBlock(NES::GlobalMemoryLocation const& where)
{
    // Attempt to split the content block at `where`. It has to lie on a data type boundary
    auto content_block = GetContentBlockAt(where);
    if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
        cout << "[ProgramRomBank::SplitContentBlock] Unupported trying to split non-data block at " << where << endl;
        return nullptr;
    }

    // Make sure the split is aligned
    u16 split_offset = ConvertToOffset(where.address);
    if((split_offset % content_block->GetDataTypeSize()) != 0) {
        cout << "[ProgramRomBank::SplitContentBlock] Illegal split at non-aligned boundary at " << where << " requiring alignment " << content_block->GetDataTypeSize() << endl;
        return nullptr;
    }

    // save the old data pointer
    void* data_ptr = content_block->data.ptr;
    assert(data_ptr != nullptr);
    u32 old_count = content_block->data.count;
    
    // create a new one and copy the beginning data do it
    u32 left_size = split_offset - content_block->offset;
    content_block->data.count = left_size / content_block->GetDataTypeSize();
    content_block->data.ptr = new u8[left_size];
    memcpy(content_block->data.ptr, data_ptr, left_size);

    // the remaining data has to go into a new content block
    shared_ptr<ContentBlock> right_block = make_shared<ContentBlock>();
    right_block->type = CONTENT_BLOCK_TYPE_DATA;
    right_block->offset = split_offset;
    right_block->data.type = content_block->data.type;
    right_block->data.count = old_count - content_block->data.count;

    // allocate the storage for the data and copy over the right side
    u32 right_size = right_block->GetSize();
    right_block->data.ptr = new u8[right_size];
    memcpy(right_block->data.ptr, (u8*)data_ptr + left_size, right_size);

    // free old memory
    delete [] data_ptr;

    // return the new object (this leaves content_ptrs out of date, so you better fix them!)
    return right_block;
}

void ProgramRomBank::MarkContentAsData(NES::GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE new_data_type)
{
    auto content_block = GetContentBlockAt(where);
    if(content_block->type != CONTENT_BLOCK_TYPE_DATA) {
        cout << "[ProgramRomBank::MarkContentAsData] TODO right now can only split other data blocks" << endl;
        return;
    }

    if(content_block->data.type == new_data_type) {
        cout << "[ProgramRomBank::MarkContentAsData] Content is already of data type " << new_data_type << endl;
        return;
    }

    // Verify the region fits within this content block
    u16 start_offset = ConvertToOffset(where.address);
    if(byte_count > (content_block->GetSize() - start_offset)) {
        cout << "[ProgramRomBank::MarkContentAsData] Error trying to mark too much data" << endl;
        return;
    }

    // OK, this content block can have the data split. The first part is to split the bank at the start of the new data
    content_block = SplitContentBlock(where);
    if(!content_block) return;
    assert(start_offset == content_block->offset);

    // The new block needs to be added into the system
    AddContentBlock(content_block);

    // This new block may need to be truncated!
    if(byte_count < content_block->GetSize()) {
        GlobalMemoryLocation end_where = where + byte_count;
        shared_ptr<ContentBlock> next_block = SplitContentBlock(end_where);

        if(!next_block) return;
        assert(ConvertToOffset(end_where.address) == next_block->offset);

        // This final block won't be used, but needs to be added to the system
        AddContentBlock(next_block);

        // And let's just refetch the working block JIC
        content_block = GetContentBlockAt(where);
    }

    // now we can convert the data type and update count
    u32 size = content_block->GetSize();
    content_block->data.type = new_data_type;
    content_block->data.count = size / content_block->GetDataTypeSize();

    cout << "finished" << endl;
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
        if(blk->type == CONTENT_BLOCK_TYPE_DATA && blk->data.ptr != NULL) {
            delete [] blk->data.ptr;
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
    blk->data.ptr = new u8[size];
    memcpy(blk->data.ptr, data, size);

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
