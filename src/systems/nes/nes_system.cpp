#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>

#include "systems/nes/nes_cartridge.h"
#include "systems/nes/nes_content.h"
#include "systems/nes/nes_memory.h"
#include "systems/nes/nes_system.h"

#include "util.h"

using namespace std;

namespace NES {

System::System()
    : ::BaseSystem()
{
}

System::~System()
{
}

bool System::CreateNewProjectFromFile(string const& file_path_name)
{
    cout << "[NES::System] CreateNewProjectFromFile begin" << endl;
    create_new_project_progress->emit(shared_from_this(), false, 0, 0, "Loading file...");

    cartridge = make_shared<Cartridge>();

    // Read in the iNES header
    ifstream rom_stream(file_path_name, ios::binary);
    if(!rom_stream) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Could not open file");
        return false;
    }

    unsigned char buf[16];
    rom_stream.read(reinterpret_cast<char*>(buf), 16);
    if(!rom_stream || !(buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        create_new_project_progress->emit(shared_from_this(), true, 0, 0, "Error: Not an NES ROM file");
        return false;
    }
 
    // Parse the iNES header
    cartridge->header.num_prg_rom_banks = (u8)buf[4];
    cartridge->header.num_chr_rom_banks = (u8)buf[5];
    cartridge->header.prg_rom_size      = cartridge->header.num_prg_rom_banks * 16 * 1024;
    cartridge->header.chr_rom_size      = cartridge->header.num_chr_rom_banks *  8 * 1024;
    cartridge->header.mapper            = ((u8)buf[6] & 0xF0) >> 4 | ((u8)buf[7] & 0xF0);
    cartridge->header.mirroring         = ((u8)buf[6] & 0x08) ? MIRRORING_FOUR_SCREEN : ((bool)((u8)buf[6] & 0x01) ? MIRRORING_VERTICAL : MIRRORING_HORIZONTAL);
    cartridge->header.has_sram          = (bool)((u8)buf[6] & 0x02);
    cartridge->header.has_trainer       = (bool)((u8)buf[6] & 0x04);

    // Allocate storage and initialize cart based on header information
    cartridge->Prepare();

    // skip the trainer if present
    if(cartridge->header.has_trainer) rom_stream.seekg(512, rom_stream.cur);

    // we now know how many things we need to load
    u32 num_steps = cartridge->header.num_prg_rom_banks + cartridge->header.num_chr_rom_banks + 1;
    u32 current_step = 0;

    // Load the PRG banks
    for(u32 i = 0; i < cartridge->header.num_prg_rom_banks; i++) {
        stringstream ss;
        ss << "Loading PRG ROM bank " << i;
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the PRG rom data
        unsigned char data[16 * 1024];
        cout << rom_stream.tellg() << endl;
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading PRG-ROM");
            return false;
        }

        // Get the PRG-ROM bank
        auto prg_bank = cartridge->GetProgramRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        prg_bank->InitializeWithData(0, sizeof(data), reinterpret_cast<u8*>(data)); // Mark content starting at offset 0 as data
    }

    // Load the CHR banks
    for(u32 i = 0; i < cartridge->header.num_chr_rom_banks; i++) {
        stringstream ss;
        ss << "Loading CHR ROM bank " << i;
        create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, ss.str());

        // Read in the CHR rom data
        unsigned char data[8 * 1024]; // TODO read 4K banks with other mappers (check chr_bank first!)
        rom_stream.read(reinterpret_cast<char *>(data), sizeof(data));
        if(!rom_stream) {
            create_new_project_progress->emit(shared_from_this(), true, num_steps, current_step, "Error: file too short when reading CHR-ROM");
            return false;
        }

        // Get the CHR bank
        auto chr_bank = cartridge->GetCharacterRomBank(i); // Bank should be empty with no content

        // Initialize the entire bank as just a series of bytes
        chr_bank->InitializeWithData(0, sizeof(data), reinterpret_cast<u8*>(data)); // Mark content starting at offset 0 as data
    }

    create_new_project_progress->emit(shared_from_this(), false, num_steps, ++current_step, "Done");
    cout << "[NES::System] CreateNewProjectFromFile end" << endl;
    return true;
}

bool System::IsROMValid(std::string const& file_path_name, std::istream& is)
{
    unsigned char buf[16];
    is.read(reinterpret_cast<char*>(buf), 16);
    if(is && (buf[0] == 'N' && buf[1] == 'E' && buf[2] == 'S' && buf[3] == 0x1A)) {
        return true;
    }

    return false;
}

// Memory
void System::GetEntryPoint(GlobalMemoryLocation* out)
{
    assert((bool)cartridge);
    zero(out);
    out->address      = 0xFFFC;
    out->prg_rom_bank = cartridge->GetResetVectorBank();
}

shared_ptr<ContentBlock>& System::GetContentBlockAt(GlobalMemoryLocation const& where)
{
    if(where.address >= 0x8000) {
        return cartridge->GetContentBlockAt(where);
    }

    static shared_ptr<ContentBlock> empty_ptr;
    return empty_ptr;
}

void System::MarkContentAsData(GlobalMemoryLocation const& where, u32 byte_count, CONTENT_BLOCK_DATA_TYPE data_type)
{
    // TODO right now we only work with ROM banks
    if(where.address >= 0x8000) {
        cartridge->MarkContentAsData(where, byte_count, data_type);
    }
}

// Listings
void System::GetListingItems(GlobalMemoryLocation const& where, std::vector<std::shared_ptr<ListingItem>>& out)
{
    assert(!where.is_chr); // TODO support CHR

    if(where.address >= 0x8000) {
        auto prg_bank = cartridge->GetProgramRomBank(where.prg_rom_bank);

        auto content_block = prg_bank->GetContentBlockAt(where);
        if(!content_block) {
            // memory not present, add a ListingUnknown to out
            auto unk = make_shared<ListingItemUnknown>(where);
            out.push_back(unk);
        }

        switch(content_block->type) {
        case CONTENT_BLOCK_TYPE_DATA:
        {
            auto dataitem = make_shared<ListingItemData>(where, prg_bank);
            out.push_back(dataitem);
            break;
        }

        case CONTENT_BLOCK_TYPE_CODE:
        case CONTENT_BLOCK_TYPE_CHR:
        default:
            assert(false); // TODO block types
        }
    }
}

u16 System::GetSegmentBase(GlobalMemoryLocation const& where)
{
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return where.address & ~0x7FF;
    } else if(where.address < 0x4020) {
        return 0x2000;
    } else if(where.address < 0x6000) {
        return 0x4020;
    } else if(where.address < 0x8000) {
        return 0x6000;
    } else {
        auto prg_bank = cartridge->GetProgramRomBank(where.prg_rom_bank);
        return prg_bank->GetBaseAddress();
    }
}

u32 System::GetSegmentSize(GlobalMemoryLocation const& where)
{
    assert(!where.is_chr); // TODO

    if(where.address < 0x2000) {
        return 0x800;
    } else if(where.address < 0x4020) {
        return 0x2020;
    } else if(where.address < 0x6000) {
        return 0x1FE0;
    } else if(where.address < 0x8000) {
        return 0x2000;
    } else {
        auto prg_bank = cartridge->GetProgramRomBank(where.prg_rom_bank);
        return prg_bank->GetRegionSize();
    }
}

BaseSystem::Information const* System::GetInformation()
{
    return System::GetInformationStatic();
}

BaseSystem::Information const* System::GetInformationStatic()
{
    static BaseSystem::Information information = {
        .abbreviation  = "NES",
        .full_name     = "Nintendo Entertainment System",
        .is_rom_valid  = std::bind(&System::IsROMValid, placeholders::_1, placeholders::_2),
        .create_system = std::bind(&System::CreateSystem)
    };

    return &information;
}

shared_ptr<BaseSystem> System::CreateSystem()
{
    System* nes_system = new System();
    return shared_ptr<BaseSystem>(nes_system);
}

}
